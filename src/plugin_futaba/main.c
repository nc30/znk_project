#include <Moai_plugin_dev.h>
#include <Znk_stdc.h>
#include <Znk_def_util.h>
#include <Znk_cookie.h>
#include <Znk_htp_hdrs.h>
#include <Znk_str_ptn.h>
#include <string.h>

static ZnkVarp
refPostVar( ZnkMyf myf, const char* var_name )
{
	return ZnkMyf_refVar( myf, "post_vars", var_name );
}
static ZnkVarp
refCookieVar( ZnkMyf myf, const char* var_name )
{
	return ZnkMyf_refVar( myf, "cookie_vars", var_name );
}

/***
 * myfのUSERS_passwordは、ユーザからの明示的な削除キー指定をシミュレーションするための
 * ものである. pwdはpwdcの値を元に自動設定される場合があり、ユーザからの明示的な指定か
 * 否かの区別がつかないため、このような変数を別途導入してある.
 */
static void
update_pwd( ZnkVarp pwd, ZnkVarp pwdc, ZnkVarp USERS_password )
{
	if( pwd ){
		if( USERS_password && !ZnkS_empty(ZnkVar_cstr(USERS_password)) ){
			/***
			 * この場合、以後pwdcもUSERS_passwordの値として発行されるはずである.
			 */
			ZnkVar_set_val_Str( pwd, ZnkVar_cstr(USERS_password),
					Znk_MIN(ZnkVar_str_leng(USERS_password),8) );
		} else if( pwdc ){
			/***
			 * pwdcがcookie_vars上において設定されているなら、この値を採用.
			 */
			ZnkVar_set_val_Str( pwd, ZnkVar_cstr(pwdc), ZnkVar_str_leng(pwdc) );
		} else {
			/***
			 * pwdcがcookie_vars上において設定されていないなら、空値とする.
			 * この場合、以後pwdcがふたばから適当な値として自動発行発行されるはずである.
			 */
			ZnkVar_set_val_Str( pwd, "", 0 );
		}
	}
}

bool on_post( ZnkMyf myf )
{
	ZnkVarp USERS_futabapt = refPostVar( myf, "USERS_futabapt" );
	ZnkVarp USERS_password = refPostVar( myf, "USERS_password" );
	ZnkVarp pthb = refPostVar( myf, "pthb" );
	ZnkVarp pthc = refPostVar( myf, "pthc" );
	ZnkVarp pwd  = refPostVar( myf, "pwd" );
	ZnkVarp pwdc = refCookieVar( myf, "pwdc" );

	ZnkF_printf_e( "plugin_futaba : on_post_before\n" );
	/***
	 * myfにUSERS_futabaptが定義されていない場合は
	 * localStorageが無効である環境と同挙動となる.
	 */
	if( USERS_futabapt ){
		/***
		 * ptfk関数のエミュレート.
		 * オリジナルでは右辺が空のとき、左辺への代入を行わないようにしてあるが、
		 * ここでは無条件で更新する. 左辺の古い値を残してはならないためである.
		 */
		if( pthb ){
			ZnkVar_set_val_Str( pthb, ZnkVar_cstr(USERS_futabapt), ZnkVar_str_leng(USERS_futabapt) );
		}
		if( pthc ){
			ZnkVar_set_val_Str( USERS_futabapt, ZnkVar_cstr(pthc), ZnkVar_str_leng(pthc) );
		}
	}

	/***
	 * これによりブラウザ上にあるpwdの値は遮断され、
	 * myf上にある値が替わりに採用される.
	 */
	update_pwd( pwd, pwdc, USERS_password );
	return true;
}

static size_t
replaceStr( ZnkStr str, size_t begin,
		const char* old_ptn, size_t old_ptn_leng,
		const char* new_ptn, size_t new_ptn_leng,
		size_t seek_depth )
{
	return ZnkStrPtn_replace( str, begin,
			(uint8_t*)old_ptn, old_ptn_leng,
			(uint8_t*)new_ptn, new_ptn_leng,
			seek_depth );
}

bool on_response( ZnkMyf myf,
		ZnkVarpAry hdr_vars, ZnkStr text, const char* req_urp )
{
	if( text ){
		const char* old_ptn = "＠ふたば</span>";
		const char* new_ptn = "＠ふたば via Moai</span>";
		replaceStr( text, 0,
				old_ptn, Znk_NPOS,
				new_ptn, Znk_NPOS,
				Znk_NPOS );
	}
	return true;
}

