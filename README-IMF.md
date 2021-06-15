# FFMPEG IMF

## Debug build

`./configure --enable-libxml2 --enable-debug --enable-static --disable-optimizations --prefix=$PWD/dist`

## Install clang-format-12 on ubuntu

```sh
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository -y "deb https://apt.llvm.org/bionic/ llvm-toolchain-bionic-12 main"
sudo apt update -q
sudo apt install -y clang-format-12
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-12 100
```