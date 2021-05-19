sudo apt-get update

sudo apt-get install make gcc autoconf autogen pkg-config libtool libssl-dev libevent-dev

git clone https://github.com/mskd12/tlsdate.git

cd tlsdate
./autogen.sh
./configure
make
sudo make install
