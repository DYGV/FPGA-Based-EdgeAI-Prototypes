# 姿勢推定
## セットアップ
### Alveo U50の場合
#### 手順
1. 環境変数設定  
  `source /workspace/setup/alveo/setup.sh DPUCAHX8L`  
2. openposeモデルのダウンロード  
  Vitis-AIに含まれる`downloader.py`により、`DPUCAHX8L`向けopenposeをダウンロードする(`DPUCAHX8H`より`DPUCAHX8L`のほうがフレームレートが高かったため)。  
3. ビルド  
  `bash -x ./build.sh`  

### ZynqMPの場合
#### 前提条件  
- Arm Cortex-A53のクロスコンパイラでコンパイルすること(コンパイラはPetalinux SDK 2022.2に付属)  
- Vitis AI Model Zooよりopenpose(画像サイズ368\*368向け)をダウンロードし、モデルを`DPUCZDX8G`向けにコンパイル済みであること  

#### 手順
1. 環境変数設定  
  ```
     unset LD_LIBRARY_PATH  
     source <petalinux_sdk_2022.2インストール先>/environment-setup-cortexa72-cortexa53-xilinx-linux
  ```  
1. ビルド  
  `bash -x ./build.sh`  
2. ZynqMPへ転送  
  `scp -r  ../openpose/ user@zynqmp:/home/user/`  


## 実行
### ローカル姿勢推定(1)
実行環境にある画像で姿勢推定をする。コマンドライン引数に機械学習モデル(openpose)ファイルのパスと、ポーズ画像が含まれるディレクトを指定する。入力画像のサイズは、368\*368である。  
実行環境にある画像で姿勢推定をする。コマンドライン引数に機械学習モデル(openpose)ファイルのパスと、画像ファイル、出力先のjsonファイルのパスを指定する。入力画像のサイズは、368\*368である。  
`./build/pose_estimation_simple openpose.xmodel 姿勢画像.jpg output.json`  

### ローカル姿勢推定(2)
実行環境にある画像を連続的に読み込み姿勢推定し結果を標準出力する。コマンドライン引数に機械学習モデル(openpose)のファイルパスと、姿勢画像のディレクトリを指定する。入力画像は、`.jpg`または`.png`とし、サイズは、368\*368である。  
`./build/pose_estimation_seq openpose.xmodel pose_frame/`  

### リモート姿勢推定
クライアントサーバ方式で姿勢推定をする。クライアント側は動画の画像フレームをサーバに送信し、サーバ側はそれを受信し姿勢推定する。レスポンスとして姿勢推定の結果を返す。クライアント側はビルド時に生成された`client`だけでなく、[ROS 2ノード(別リポジトリ)](https://github.com/DYGV/ros2tcp-edgeAI)からサーバへ接続することも可能である。  
  - サーバ  
    コマンドライン引数に機械学習モデル(openpose)ファイルのパスと、サーバのポート番号を指定する。レスポンスとして部位座標をクライアント側にjson形式で返す。  
    `./build/pose_estimation_server openpose.xmodelパス 54321`  
  - クライアント  
    コマンドライン引数にサーバのIPアドレスと、サーバのポート番号、動画ファイルのディレクトリを指定する。入力動画のサイズは、368\*368である。  
    `./build/client ***.***.*** 54321 動画ファイル.mp4`  

