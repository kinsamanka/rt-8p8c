The PIC32 firmware is compiled using the compiler provided by MPIDE.

## Steps ##

  * Download the latest version of the  [MPIDE compiler](https://github.com/chipKIT32/chipKIT32-MAX/downloads).
  * Extract and move the compiler to `/usr/local`
```

tar --wildcards -xvzf mpide-0023-linux-*.tgz  'mpide-*/hardware/pic32/compiler/pic32-tools'
sudo mv mpide-*/hardware/pic32/compiler/pic32-tools /usr/local
rm  mpide-*/ -r
```
  * Done!