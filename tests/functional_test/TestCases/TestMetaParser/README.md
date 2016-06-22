![alt tag](https://gitlab.hopebaytech.com/gateway-2-0/hcfs/tree/test-meta-parser/tests/functional_test/TestCases/TestMetaParser/arch.jpg)

以"start_test_meta_parser.sh"開始執行測試,步驟
1. 檢查是否安裝 docker 若無則停止執行
2. 若尚未建立 docker image 則建立 docker image, 建立時於 image 內以 setup_dev_env.sh 檢查並安裝必要套件
3. 呼叫 python script DockerTest.py, 執行測試用的 docker 設定在 DockerTest.py and DockerMgt.py
4. docker 內呼叫 pi_tester.py 執行 TestMetaParser.py

swift : 使用 Arkflex U 的 server 以 Ted 的帳號執行
hcfs : 執行於 docker 上


更改測試環境請於 Env.py
路徑參數於 Var.py

meta_parser.c Makefile lib.so
meta_parser.c C 轉 python 的測試 API 實作,目前為 dummy
以make編譯,輸出 lib.so, 使用時 import lib 即可使用,函式名稱參照 spec
