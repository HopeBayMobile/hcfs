執行需要 root 權限

目前環境是使用 Arkflex U 的 server 以 Ted 的帳號執行 local 端的 hcfs;
更改測試環境請於 Env.py
路徑參數於 Var.py

meta_parser.c Makefile lib.so
meta_parser.c C 轉 python 的測試 API 實作,目前為 dummy
以make編譯,輸出 lib.so, 使用時 import lib 即可使用,函式名稱參照 spec
