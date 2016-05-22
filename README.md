# znk_project
===================================

This is Japanese overview. English version is [here][7].

## 概要 znk_projectとは?
-----------------------------------

"znk_project" はフリーかつオープンソースなアプリケーションおよびライブラリを開発/提供するリポジトリです.
これらはすべてlibZnkという基本ライブラリによってWindows またはLinux向けに開発されたものであります.

現在このリポジトリで提供しておりますものは以下の通り:

* libZnk : 基本的なC/C++用ライブラリ  
  ドキュメントは[こちら][1]です.
* Moai: また一つ生まれた(ばかりの)ローカルプロキシエンジン  
  ドキュメントは[こちら][2]です.
  リファレンスマニュアルは[こちら][3]です.
* Moai/Virtual USERS機能: Moaiを使って掲示板のユーザ情報を仮想化する方法について  
  ドキュメントは[こちら][4]です.
* http_decorator : シンプルなHTTPクライアント.  
...などなど 

これらのアプリケーションおよびライブラリのソースコードはVC(Ver6.0からVer12.0)、MinGW、(linux上の)gccなど向けに
書かれたものです. コンパイル方法に関しては[こちら][6]をご覧ください.

Download & Support site:  
https://github.com/mr-moai-2016/znk_project


## What's new?
-----------------------------------

* 2016/05/21 : 正式バージョン1.0がリリースされました.  
  リリースノートは[こちら][5]です.


## License
-----------------------------------

zlib 1.2.3 :   
  Copyright (C) 1995-2005 Jean-loup Gailly and Mark Adler  

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  [NOTE]  
  libZnk use zlib as statically sub-object.
  zlib in here, its source code itself is not at all modified from its original version,
  but accessory of example and document files are deleted except for the license terms
  (README) to avoid redundancy.

Otherwise all the code :  
  Copyright (c) Nippon HTTP Kenkyujo(NHK)
  Licensed under the NYSL( see http://www.kmonos.net/nysl/index.en.html for detail ).


This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising
from the use of this software.


[1]: https://github.com/mr-moai-2016/znk_project/blob/master/src/libZnk/README.md
[2]: https://github.com/mr-moai-2016/znk_project/blob/master/src/moai/README.md
[3]: https://github.com/mr-moai-2016/znk_project/blob/master/src/moai/Reference.md
[4]: https://github.com/mr-moai-2016/znk_project/blob/master/src/virtual_users/VirtualUSERS.md
[5]: https://github.com/mr-moai-2016/znk_project/blob/master/src/ReleaseNote.md
[6]: https://github.com/mr-moai-2016/znk_project/blob/master/src/HowToCompile.md
[7]: https://github.com/mr-moai-2016/znk_project/blob/master/README_en.md
