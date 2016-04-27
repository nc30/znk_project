#!/bin/sh

ROOT_DIR_="$1"
if test "$ROOT_DIR_" = "" ; then
	ROOT_DIR_=".."
fi

CP_="install -p"
CP_COMFIRM_="cp -i"
INST_DIR_=$ROOT_DIR_/bin_for_linux

mkdir -p "$INST_DIR_"

#
# copy libZnk so
#
if test -e libZnk ; then
	SRC_DIR=libZnk
fi

DL_VER_SFX=
$CP_ $SRC_DIR/out_dir/libZnk$DL_VER_SFX.so $INST_DIR_/

#
# setup moai
#
$CP_ moai/out_dir/moai $INST_DIR_/
mkdir -p "$INST_DIR_/doc_root"
LIST=`ls moai/doc_root`
for i in $LIST
do
	$CP_ moai/doc_root/$i $INST_DIR_/doc_root/
done

#
# setup http_decorator
#
$CP_ http_decorator/out_dir/http_decorator $INST_DIR_/

#
# setup virtual_users
#
install_one()
{
	if_dst_exist=$1
	file=$2
	src_dir=$3
	dst_dir=$4
	if   test "$if_dst_exist" = "-f" ; then
		$CP_ $src_dir/$file $dst_dir/
	elif test "$if_dst_exist" = "-n" ; then
		if ! test -e "$dst_dir/$file" ; then
			$CP_ $src_dir/$file $dst_dir/
		fi
	elif test "$if_dst_exist" = "-c" ; then
		$CP_COMFIRM_ $src_dir/$file $dst_dir/
	else
	fi
}
$CP_ virtual_users/out_dir/virtual_users $INST_DIR_/
install_one -c user_agent.txt  virtual_users $INST_DIR_
install_one -c screen_size.txt virtual_users $INST_DIR_
mkdir -p $INST_DIR_/filters
LIST=`ls virtual_users/filters`
for i in $LIST
do
	install_one -c $i virtual_users/filters $INST_DIR_/filters
done
install_one -c config.myf      virtual_users $INST_DIR_
install_one -c analysis.myf    virtual_users $INST_DIR_
install_one -c target.myf      virtual_users $INST_DIR_
install_one -c README.txt      virtual_users $INST_DIR_
install_one -c README_more.txt virtual_users $INST_DIR_

#
# setup plugin_futaba
#
mkdir -p "$INST_DIR_/plugins"
$CP_ plugin_futaba/out_dir/futaba.so $INST_DIR_/plugins/

echo "All files installed to $INST_DIR_ successfully."
