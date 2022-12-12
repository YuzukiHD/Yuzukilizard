1. These files are sample code to test Openssl/ce of allwinner

2. cd custom_aw/lib , then enter "mk -B"

3. If you want to add new test sample, please modify build_test.mk then enter "mk build_test"

4. If you want to generate libawcrypt.so enter "mk -B"

5. boot the board and cd /lib/modules/`uname -r`/  
   insmod ss.ko
   insmod af_alg.ko 
   insmod algif_hash.ko
   insmod algif_skcipher.ko
   insmod algif_rng.ko
   Take note!!! : for V40 you must insert sha256_generic.ko because of aw trng implement,
   for V5 it is optional.

6. crypto_tapp
