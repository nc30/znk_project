#include <Znk_htp_rar.h>
#include <Znk_net_base.h>
#include <Znk_socket.h>
#include <Znk_cookie.h>
#include <Znk_s_base.h>
#include <Znk_str_ary.h>
#include <Znk_stdc.h>
#include <Znk_mem_find.h>
#include <Znk_sys_errno.h>
#include <Znk_str_ex.h>
#include <Znk_stock_bio.h>
#include <Znk_zlib.h>

#include <stdio.h>
#include <string.h>

typedef int (*ZnkSenderFunc)( void* arg, uint8_t* buf, size_t buf_size );

static void
printLastRecvError( int line_idx )
{
	ZnkF_printf_e( "ZnkHtpRAR : Error : line=%d : ZnkSocket_recv error : [%s]\n",
			line_idx, ZnkNetBase_getErrKey(ZnkNetBase_getLastErrCode()) );
}

typedef struct HeaderInfo_tag {
	bool       is_chunked_;
	size_t     content_length_;
	bool       is_gzip_;
	ZnkStrAry set_cookie_; 
} HeaderInfo;

static HeaderInfo*
createHeaderInfo( void )
{
	HeaderInfo* hdr_info = Znk_malloc( sizeof( struct HeaderInfo_tag ) );
	hdr_info->is_chunked_     = false;
	hdr_info->content_length_ = 0;
	hdr_info->is_gzip_        = false;
	hdr_info->set_cookie_     = ZnkStrAry_create( true );
	return hdr_info;
}
static void
clearHeaderInfo( HeaderInfo* hdr_info )
{
	hdr_info->is_chunked_     = false;
	hdr_info->content_length_ = 0;
	hdr_info->is_gzip_        = false;
	ZnkStrAry_clear( hdr_info->set_cookie_ );
}
static void
destroyHeaderInfo( HeaderInfo* hdr_info )
{
	if( hdr_info ){
		ZnkStrAry_destroy( hdr_info->set_cookie_ );
		Znk_free( hdr_info );
	}
}

static bool
appendRecvedData( ZnkStockBIO sbio, ZnkBfr dst, size_t data_size )
{
	uint8_t buf[ 4096 ];
	size_t readed_size;
	while( data_size ){
		if( data_size > sizeof(buf) ){
			readed_size = ZnkStockBIO_read( sbio, buf, sizeof(buf) );
		} else {
			readed_size = ZnkStockBIO_read( sbio, buf, data_size );
		}
		if( readed_size == 0 ){
			/* end of recv */
			return false;
		} else if( readed_size < 0 ){
			/* error of recv */
			printLastRecvError( __LINE__ );
			return false;
		}
		data_size -= readed_size;
		ZnkBfr_append_dfr( dst, buf, readed_size );
	}
	return true;
}

/****
 * chunkedモードの場合、"hello_world" というbodyは以下のような形のデータとなっている.
 *
 * HTTP/1.1 200 OK
 * Content-Type: text/html
 * Transfer-Encoding: chunked
 * 
 * 6
 * hello_
 * 5
 * world
 * 0
 *
 * chunk_sizeの後に\r\nがあるのはもちろんであるが、その後chunk_size byte進んだところにも\r\nがあることに注意.
 * その後に次のchunk_sizeが来る.
 */
static bool
appendBody( ZnkStockBIO sbio, ZnkBfr body, const HeaderInfo* hdr_info )
{
	/***
	 * 注:
	 * recvからの戻り値が 0 の場合、0 より大きい場合に比べてかなり長い待ち時間が発生する可能性がある.
	 * それを回避するため、なるべく終了のタイミングをrecvに頼らないようにする.
	 */
	if( hdr_info->is_chunked_ ){
		size_t chunk_size = 0;
		char chunk_size_line[ 1024 ];
		ZnkF_printf_e( "ZnkHtpRAR : [chunked mode]\n" );

		while( true ){
			/* chunk_size_lineを読み込み */
			ZnkStockBIO_readLineCRLF( sbio, chunk_size_line, sizeof(chunk_size_line) );
			/* chunk_size は 16進数で表記されている */
			if( !ZnkS_getSzX( &chunk_size, chunk_size_line ) ){
				/* error */
			}
			/***
			 * chunkedモードにおいては、結びのchunk_sizeは必ず0になっている.
			 * また無事これに到達したならresultは成功である.
			 */
			if( chunk_size == 0 ){
				/* success */
				break;
			}

			if( !appendRecvedData( sbio, body, chunk_size ) ){
				/* error */
			}

			/* chunk dataの一番最後にもCRLFがある. これを読み飛ばす */
			ZnkStockBIO_readLineCRLF( sbio, chunk_size_line, sizeof(chunk_size_line) );
		}

	} else if( hdr_info->content_length_ > 0 ){
		ZnkF_printf_e( "ZnkHtpRAR : [content_length(%u) mode]\n", hdr_info->content_length_ );
		appendRecvedData( sbio, body, hdr_info->content_length_ );

	} else {
		uint8_t buf[ 4096 ];
		size_t readed_size;
		ZnkF_printf_e( "ZnkHtpRAR : [unknown mode]\n" );
		/***
		 * この場合、終了のタイミングはrecvからの0戻り値に頼るほかない.
		 */
		while( true ){
			readed_size = ZnkStockBIO_read( sbio, buf, sizeof(buf) );
			if( readed_size < 0 ){
				/* error */
				break;
			}
			if( readed_size == 0 ){
				/* これで一応成功とみなす... */
				break;
			}
			ZnkBfr_append_dfr( body, buf, readed_size );
		}
	}
	return true;
}

/***
 * まず全Headerを得るまでSock通信によりrecvを繰り返す.
 * その際、sizeof(buf) byteずつ受信するが、これらはfirst_recved_bufという形で
 * 蓄積して取得し、Header全体を網羅した時点でrecvは中断する.
 * この関数はfirst_recved_bufを返すが、これの残りには通常bodyの開始部分が含まれて
 * いるはずなので、後に下流過程の関数でこの部分を参照する.
 * また同時にHeaderの全文字列とHeaderの終了バイト位置も取得する.
 */
static int
recvHeader( ZnkStockBIO sbio, ZnkHtpHdrs recv_hdrs, HeaderInfo* hdr_info )
{
	char buf[ 8192 ] = { 0 };
	size_t readed_size;

	if( !ZnkStockBIO_readLineCRLF( sbio, buf, sizeof(buf) ) ){
		/***
		 * 通信に関するエラーが発生.
		 * これが発生することはあまりないが、発生した場合は WSAECONNRESET( ピア側での接続がリセットされました ) 
		 * が最も典型的なものである. これは通信している相手が強引に接続を断ったことを意味する.
		 */
		printLastRecvError( __LINE__ );
		return -1;
	}

	ZnkHtpHdrs_registHdr1st( recv_hdrs->hdr1st_, buf, strlen(buf) );

	/* 最初が "HTTP/" ではじまるかどうかを確認 */
	if( !ZnkS_isBegin( buf, "HTTP/" ) ){
		/* プロトコルがHTTPとは異なる */
		ZnkF_printf_e( "ZnkHtpRAR : Error : recved status line : [%s]\n", (char*)buf );
		ZnkF_printf_e( "            : This is not begun by \"HTTP/\"\n" );
		return -1;
	}

	/***
	 * header Key:Val部読み込み
	 */
	clearHeaderInfo( hdr_info );

	{
		/* HTTP versionを確認 */
		char* ptr = (char*)( buf + 5 );
		if( strncmp("1.0 ", ptr, 4) == 0 ){
			ptr += 4;
		} else if( strncmp("1.1 ", ptr, 4) == 0 ){
			ptr += 4;
		} else {
			ZnkF_printf_e( "ZnkHtpRAR : unknown HTTP version\n" );
			return -1;
		}
	
		/* 次の項目(トークン)までスペースをスキップ */
		while( *ptr == ' ' ){ ++ptr; }
	
		/***
		 * HTTPの結果を解析
		 */
		{
			int status_code = 0;
			sscanf( ptr, "%d", &status_code );
			switch( status_code ){
			case 200:
				break;
			case 302:
				ZnkF_printf_e( "ZnkHtpRAR : This is 302 moved.\n" );
				break;
			case 404:
				ZnkF_printf_e( "ZnkHtpRAR : This is 404 not found.\n" );
				break;
			default:
				ZnkF_printf_e( "ZnkHtpRAR : This is status_code[%d].\n", status_code );
				break;
			}
		}
	}

	while( ZnkStockBIO_readLineCRLF( sbio, buf, sizeof(buf) ) ){
		if( ZnkS_empty( buf ) ){
			/* ヘッダ部終わりのCRLFであると思われる */
			break;
		}
		//parseHeaderInfo( buf, hdr_info, recv_hdrs );
		ZnkHtpHdrs_regist_byLine( recv_hdrs->vars_, buf, Znk_NPOS );
	}

	readed_size = ZnkStockBIO_getReadedSize( sbio );
	if( readed_size < 0 ){
		printLastRecvError( __LINE__ );
		return -1;
	}

	{
		ZnkVarp     var = NULL;
		const char* val = NULL;

		var = ZnkHtpHdrs_find_literal( recv_hdrs->vars_, "Transfer-Encoding" );
		if( var ){
			val = ZnkHtpHdrs_val_cstr( var, 0 );
			if( ZnkS_eq( val, "chunked" ) ){
				hdr_info->is_chunked_ = true;
			}
		}
		var = ZnkHtpHdrs_find_literal( recv_hdrs->vars_, "Content-Length" );
		if( var ){
			val = ZnkHtpHdrs_val_cstr( var, 0 );
			if( !ZnkS_empty( val ) ){
				ZnkS_getSzU( &hdr_info->content_length_, val );
			}
		}

		var = ZnkHtpHdrs_find_literal( recv_hdrs->vars_, "Content-Encoding" );
		if( var ){
			val = ZnkHtpHdrs_val_cstr( var, 0 );
			if( ZnkS_eq( val, "gzip" ) ){
				hdr_info->is_gzip_ = true;
			}
		}

		var = ZnkHtpHdrs_find_literal( recv_hdrs->vars_, "Set-Cookie" );
		if( var ){
			const size_t val_size = ZnkHtpHdrs_val_size( var );
			size_t val_leng = 0;
			size_t idx;
			size_t pos;
			for( idx=0; idx<val_size; ++idx ){
				val = ZnkHtpHdrs_val_cstr( var, idx );
				val_leng = Znk_strlen( val );
				pos = ZnkMem_lfind_8( (uint8_t*)val, val_leng, (uint8_t)';', 1 );
				ZnkStrAry_push_bk_cstr( hdr_info->set_cookie_, val, ( pos == Znk_NPOS ) ? val_leng : pos );
			}
		}
	}
	return 0;
}

static bool
uncompressBfr( ZnkZStream zst, ZnkBfr dst_bfr, const ZnkBfr src_bfr )
{
	uint8_t dst_buf[ 4096 ];
	size_t expanded_dst_size = 0;
	size_t expanded_src_size = 0;
	const uint8_t* src;
	size_t src_size;
	src      = ZnkBfr_data( src_bfr );
	src_size = ZnkBfr_size( src_bfr );
	while( src_size ){
		if( !ZnkZStream_inflate( zst, dst_buf, sizeof(dst_buf), src, src_size,
				&expanded_dst_size, &expanded_src_size ) ){
			ZnkF_printf_e( "expanded_src_size=[%zu]\n", expanded_src_size );
			return false;
		}
		assert( expanded_src_size );
		ZnkBfr_append_dfr( dst_bfr, dst_buf, expanded_dst_size );
		src_size -= expanded_src_size;
		src += expanded_src_size;
	}
	return true;
}

static bool
recvHeaderAndBody( ZnkStockBIO sbio, HeaderInfo* hdr_info, ZnkHtpHdrs recv_hdrs, ZnkBfr body )
{
	int result;
	bool uncomp_result = true;

	result = recvHeader( sbio, recv_hdrs, hdr_info );
	if( result < 0 ){
		return false;
	}

	/***
	 * bodyを取得.
	 * HTMLといえども圧縮されbinary状態で得られることもある.
	 * 従ってここではZnkStrではなくZnkBfrを使う.
	 */
	if( !appendBody( sbio, body, hdr_info ) ){
		return false;
	}

	/***
	 * HTMLなどはgzip圧縮されているケースが見られる.
	 * しかしこれは別にHTML限定というわけでなく、一般にどんなバイナリであれ
	 * この形式で圧縮されていても構わない.
	 * いずれの場合でも、指定された圧縮形式の指示に従い、ここで本来のバイナリイメージへ
	 * 展開するだけである.
	 *
	 * 今後の課題としては、超大容量ファイルをダウンロードする場合に、少しずつファイルへ
	 * 落とすような形で保存する処理をサポートすることだが、とりあえずそれは後回し.
	 */
	if( hdr_info->is_gzip_ ){
		ZnkZStream zst = ZnkZStream_create();
		ZnkBfr dst_bfr = ZnkBfr_create_null();

		ZnkZStream_inflateInit( zst );

		uncomp_result = uncompressBfr( zst, dst_bfr, body );

		ZnkZStream_inflateEnd( zst );
		ZnkZStream_destroy( zst );

		if( uncomp_result ){
			ZnkBfr_swap( dst_bfr, body );
		}
		ZnkBfr_destroy( dst_bfr );
	}

	return uncomp_result;
}

static int
recvSocket( void* arg, uint8_t* buf, size_t buf_size )
{
	ZnkSocket sock = (ZnkSocket)arg;
	return ZnkSocket_recv( sock, buf, buf_size );
}

static bool
sendAndRecv_bySocket( ZnkSocket sock,
		ZnkHtpHdrs send_hdrs, ZnkBfr send_body,
		ZnkHtpHdrs recv_hdrs, ZnkHtpOnRecvFuncArg recv_fnca,
		ZnkVarpAry cookie, ZnkBfr wk_bfr )
{
	ZnkErr_D( err );
	bool result = false;
	bool internal_wk_bfr = false;
	ZnkStockBIO sbio = ZnkStockBIO_create( 256, recvSocket, (void*)sock );

	if( wk_bfr == NULL ){
		internal_wk_bfr = true;
		wk_bfr = ZnkBfr_create_null();
	}

	ZnkBfr_clear( wk_bfr );
	ZnkHtpHdrs_extendToStream( send_hdrs->hdr1st_, send_hdrs->vars_, wk_bfr, true );

	/* BodyImageを追加 */
	if( send_body ){
		ZnkBfr_append_dfr( wk_bfr, ZnkBfr_data(send_body), ZnkBfr_size(send_body) );
	}

	if( ZnkSocket_send( sock, ZnkBfr_data( wk_bfr ), ZnkBfr_size( wk_bfr ) ) == -1 ){
		ZnkSysErrnoInfo* err_info = ZnkSysErrno_getInfo( ZnkSysErrno_errno() );
		ZnkErr_internf( &err,
				"ZnkSocket_send : Failure : SysErr=[%s:%s]",
				err_info->sys_errno_key_, err_info->sys_errno_msg_ );
		result = false;
		goto FUNC_END;
	}

	{
		HeaderInfo* hdr_info = createHeaderInfo();

		ZnkBfr_clear( wk_bfr );

		if( !recvHeaderAndBody( sbio, hdr_info, recv_hdrs, wk_bfr ) ){
			ZnkF_printf_e( "ZnkHtpRAR : recvHeaderAndBody : [Failure].\n" );
			result = false;
		} else {
			if( cookie ){
				size_t i;
				size_t n = ZnkStrAry_size( hdr_info->set_cookie_ );
				for( i=0; i<n; ++i ){
					const char* line = ZnkStrAry_at_cstr( hdr_info->set_cookie_, i );
					ZnkCookie_regist_byAssignmentStatement( cookie, line, Znk_NPOS );
				}
			}
		
			if( recv_fnca.func_ ){
				(*recv_fnca.func_)( recv_fnca.arg_,
						ZnkBfr_data(wk_bfr), ZnkBfr_size(wk_bfr) );
			}
			result = true;
		}

		destroyHeaderInfo( hdr_info );
	}
	
FUNC_END:
	if( internal_wk_bfr ){
		ZnkBfr_destroy( wk_bfr );
	}
	ZnkStockBIO_destroy( sbio );
	return result;
}

const char*
ZnkHtpRAR_getHostnameAndUnderpath_fromURL( const char* url, char* hostname_buf, size_t hostname_buf_size )
{
	size_t slash_pos;
	const char* underpath = "";
	if( ZnkS_isBegin( url, "http://" ) ){
		url = url + 7;
	}
	slash_pos = ZnkS_lfind_one_of( url, 0, Znk_NPOS, "/", 1 );
	ZnkS_copy( hostname_buf, hostname_buf_size, url, slash_pos );
	if( slash_pos != Znk_NPOS ){
		underpath = url + slash_pos + 1;
	}
	return underpath;
}

void
ZnkHtpRAR_getHostnameAndPort( const char* url,
		char* hostname_buf, size_t hostname_buf_size, uint16_t* port )
{
	size_t colon_pos;
	size_t slash_pos;
	char host_and_port[ 512 ];

	if( ZnkS_isBegin( url, "http://" ) ){
		url = url + 7;
	} else if( ZnkS_isBegin( url, "https://" ) ){
		url = url + 8;
	}

	slash_pos = ZnkS_lfind_one_of( url, 0, Znk_NPOS, "/", 1 );
	ZnkS_copy( host_and_port, sizeof(host_and_port), url, slash_pos );

	colon_pos = ZnkS_lfind_one_of( host_and_port, 0, Znk_NPOS, ":", 1 );
	ZnkS_copy( hostname_buf, hostname_buf_size, host_and_port, colon_pos );

	if( colon_pos == Znk_NPOS ){
		if( port ){ *port = 80; }
	} else {
		if( port ){
			const char* port_str = url + colon_pos + 1;
			ZnkS_getU16U( port, port_str );
		}
	}
}

bool
ZnkHtpRAR_sendAndRecv( const char* cnct_hostname, uint16_t cnct_port,
		ZnkHtpHdrs send_hdrs, ZnkBfr send_body,
		ZnkHtpHdrs recv_hdrs, ZnkHtpOnRecvFuncArg recv_fnca,
		ZnkVarpAry cookie,
		size_t try_connect_num, bool is_proxy, ZnkBfr wk_bfr )
{
	bool        result = false;
	const char* hostname = cnct_hostname;
	bool        is_need_modify_req_uri = false;
	ZnkVarp     varp   = ZnkHtpHdrs_find_literal( send_hdrs->vars_, "Host" );
	ZnkStrAry  hdr1st = send_hdrs->hdr1st_;

	if( varp ){
		hostname = ZnkVar_name_cstr( varp );
		if( is_proxy ){
			ZnkStr req_uri = ZnkStrAry_at( hdr1st, 1 );
			/* connect via proxy. */
			switch( ZnkStr_first(req_uri) ){
			case '/':
				/* req_uri begin from under-path */
				is_need_modify_req_uri = true;
				break;
			case '*':
			default:
				/* req_uri begin from hostname or * */
				break;
			}
		}
	}

	if( is_need_modify_req_uri ){
		ZnkStr tmp = ZnkStr_new( "" );
		ZnkStr req_uri = ZnkStrAry_at( hdr1st, 1 );
		ZnkStr_addf( tmp, "%s%s", hostname, ZnkStr_cstr( req_uri ) );
		ZnkStr_swap( tmp, req_uri );
		ZnkStr_delete( tmp );
	}

	{
		ZnkErr_D( err );
		ZnkSocket sock = ZnkSocket_open();
		bool is_inprogress = false;
	
		while( try_connect_num ){
			if( !ZnkSocket_connectToServer( sock, cnct_hostname, cnct_port, &err, &is_inprogress ) ){
				ZnkF_printf_e( "%s (try=%u).\n", ZnkErr_cstr(err),try_connect_num );
				--try_connect_num;
				if( try_connect_num == 0 ){
					result = false;
					goto FUNC_END;
				}
			} else {
				break;
			}
		}
		ZnkF_printf_e( "ZnkHtpRAR : connectTo[%s:%hu] : [Success].\n", cnct_hostname, cnct_port );
	
		result = sendAndRecv_bySocket( sock,
				send_hdrs, send_body,
				recv_hdrs, recv_fnca,
				cookie, wk_bfr );

		ZnkSocket_close( sock );
	}

FUNC_END:
	return result;
}
