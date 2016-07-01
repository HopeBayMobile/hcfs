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

測試資料
- 手機為Ted.Chen手機序號為（00f28ec4cb50a4f2）adb get-serialno 確認該手機為Ted.Chen手機才會執行"動態抓取測試資料測試"
- 該手機的 Tera 對應 swift 為 swift --insecure -A https://61.219.202.83:8080/auth/v1.0 -U 005FR0018SAL:005FR0018SAL -K WligAP3pSFWS list Z4O7HP3LFPPRKRJVGV99ZRUIYBDJSWCGQ2TAS80HB1I0PC76VJ1A1CLRAROHIO86TX8UC2QL5ZENAHO0687JT5D2JA924K3BOKY7RM6YRDBBVIKOM5XWP9HJ51QWSI37
-	1. 動態抓取測試資料測試
-	directory : /sdcard/DCIM/Camera/
-	file : /sdcard/DCIM/Camera/IMG_20160623_143221.jpg
- 
-	2. 靜態測試資料（來源：事先從Ted.Chen手機抓下來資料）
-	test_data 下以資料夾區隔每一資料夾內包含兩個檔案 如：690,meta_690
-	檔案 property 為手機上以 stat 指令取得的資料


Note
- 測試環境請於 Env.py
- 參數於 VarMgt.py
