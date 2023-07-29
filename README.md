# FPGA-Based-EdgeAI-Prototypes
エッジコンピューティングのAI処理(エッジAI)を低遅延・低消費電力動作の実現に向けたプロトタイプで、FPGA実行環境内の画像ファイルを推論処理をするプログラムと、クライアントサーバ方式でFPGAマシンに接続して推論処理をするプログラムが含まれる。セットアップしたサーバは[ROS 2ノード(別リポジトリ)](https://github.com/DYGV/ros2tcp-edgeAI)から接続することも可能である。  
- [顔検出](./face_detection)
- [姿勢推定](./pose_estimation)

## 動作確認済み環境
  - Zynq UltraScale+ MPSoC カスタムボード (device part: xczu19eg-ffvc1760-2-i)
    - OS: Petalinux 2022.2
    - CPU: Cortex-A53 (2コア4スレッド)
    - Petalinux SDK: 2022.2
    - Vitis-AI: [3.0](https://github.com/Xilinx/Vitis-AI/tree/3.0)
    - DPU (Deep Learning Processor Unit): [DPUCZDX8G v4.1](https://docs.xilinx.com/r/en-US/pg338-dpu)(B4096)
    - Boost C++ Libraries: 1.77.0
  - Alveo U50
    - ホストマシンOS: Ubuntu 18.04
    - ホストマシンCPU: 12th Gen Intel(R) Core(TM) i5-12400 (6コア12スレッド)
    - Vitis-AI: [1.4.1](https://github.com/Xilinx/Vitis-AI/tree/1.4.1)
    - DPU (Deep Learning Processor Unit): [DPUCAHX8H v1.1](https://docs.xilinx.com/r/1.1-English/pg367-dpucahx8h), [DPUCAHX8L v1.0](https://docs.xilinx.com/r/en-US/pg366-dpucahx8l)
    - Boost C++ Libraries: 1.77.0

## ライセンス
このパッケージはApache License 2.0のもとで配布されています。詳細については、[LICENSEファイル](./LICENSE.md)を参照してください。  
