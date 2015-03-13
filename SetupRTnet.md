
```
  sudo apt-get build-dep emc2
```
  * Download latest stable [RTnet sources](http://www.rtnet.org/download/rtnet-0.9.13.tar.bz2) and then untar
```
  wget http://www.rtnet.org/download/rtnet-0.9.13.tar.bz2
  tar xvjf rtnet-0.9.13.tar.bz2
```
  * Configure and compile. Make sure to select the proper network driver
```
  cd rtnet-0.9.13
  ./configure --enable-8139 --with-rtext=/usr/realtime-2.6.32-122-rtai/
  make
```
  * Install to `/usr/local/rtnet`
```
  sudo make install
```