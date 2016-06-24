![alt tag](https://gitlab.hopebaytech.com/gateway-2-0/hcfs/raw/test-meta-parser/tests/functional_test/TestCases/TestMetaParser/arch.jpg)

以"tests/functional_test/start_test_meta_parser.sh"開始執行測試,步驟
1. 以 utils/setup_dev_env.sh 檢查環境
2. 呼叫 build.sh 產生 meta parser python lib 安裝檔
3. 呼叫 python tests/functional_test/TestCases/TestMetaParser/DockerTest.py 啟動 docker
4. docker 內執行 tests/functional_test/TestCases/TestMetaParser/start_test_in_docker.sh 

tests/functional_test/TestCases/TestMetaParser/start_test_in_docker.sh
1. 執行 tests/functional_test/pi_tester.py

測試環境
- swift : 使用 Arkflex U 的 server 以 Ted 的帳號執行
- hcfs : 執行於 docker 上

Note
- 測試環境請於 Env.py
- 參數於 Var.py
