sudo apt update

wget https://github.com/relic-toolkit/relic/archive/refs/tags/relic-toolkit-0.5.0.tar.gz
tar xvzf relic-toolkit-0.5.0.tar.gz

sudo apt install cmake

cd relic-relic-toolkit-0.5.0
mkdir build-gmp-bls381
cd build-gmp-bls381
../preset/gmp-pbc-bls381.sh ../
make -j
sudo make install
cd ../..

