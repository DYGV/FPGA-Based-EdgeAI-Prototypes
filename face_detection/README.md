# 顔検出
## セットアップ
### Alveo U50の場合
#### 手順
1. 環境変数設定  
  `source /workspace/setup/alveo/setup.sh DPUCAHX8H`
2. denseboxモデルのダウンロード  
  Vitis-AIに含まれる`downloader.py`により、`DPUCAHX8H`向けdenseboxをダウンロードする。
3. ビルド  
  `bash -x ./build.sh`  

### ZynqMPの場合
#### 前提条件
- Arm Cortex-A53のクロスコンパイラでコンパイルすること(コンパイラはPetalinux SDK 2022.2に付属)  
- Vitis AI Model Zooよりdensebox(画像サイズ640\*360向け)をダウンロードし、モデルを`DPUCZDX8G`向けにコンパイル済みであること  

#### 手順
1. ビルド  
  `bash -x ./build.sh`  
2. ZynqMPへ転送  
  `scp -r  ../facedetect user@zynqmp:/home/user/`  


## 実行
### ローカル顔検出(1)
実行環境にある画像で顔検出処理をする。コマンドライン引数に機械学習モデル(densebox)ファイルのパスと、顔画像ファイル、出力先のjsonファイルのパスを指定する。入力画像のサイズは、640\*360である。  
`./build/facedetect_simple densebox.xmodel 顔画像.jpg output.json`  

### ローカル顔検出(2)
実行環境にある画像を連続的に読み込み顔検出処理し結果を標準出力する。コマンドライン引数に機械学習モデル(densebox)のファイルパスと、顔画像のディレクトリを指定する。入力画像は、`.jpg`または`.png`とし、サイズは、640\*360である。  
`./build/face_detection_seq densebox.xmodel face_frame/`  

### リモート顔検出  
クライアントサーバ方式で顔検出処理をする。クライアント側は動画の画像フレームをサーバに送信し、サーバ側はそれを受信し顔検出する。レスポンスとして顔の座標・大きさをクライアント側にjson形式で返す。クライアント側はビルド時に生成された`client`だけでなく、[ROS 2ノード(別リポジトリ)](https://github.com/DYGV/ros2tcp-edgeAI)からサーバへ接続することも可能である。   
  - サーバ  
    コマンドライン引数に機械学習モデル(densebox)ファイルのパスと、サーバのポート番号を指定する。  
    `./build/facedetect_server densebox.xmodel 54321`  
  - クライアント  
    コマンドライン引数にサーバのIPアドレスと、サーバのポート番号、動画ファイルのディレクトリを指定する。入力動画のサイズは、640\*360である。  
    `./build/client ***.***.*** 54321 動画ファイル.mp4`  

