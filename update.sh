g++ main.cpp fastpixelmap.cpp decodevideo.cpp -lavutil -lavformat -lavcodec -lavfilter -lm -lz -lswscale -O2
mv a.out videoConverter
sudo mv videoConverter /usr/bin/
