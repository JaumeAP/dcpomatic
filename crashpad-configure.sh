crashpad=$HOME/src/crashpad/crashpad
export CXXFLAGS="-I$crashpad -I$crashpad/third_party/mini_chromium/mini_chromium -I$crashpad/out/Default/gen"
export LINKFLAGS="-L$crashpad/out/Default/obj/client -L$crashpad/out/Default/obj/handler -L$crashpad/out/Default/obj/util -L$crashpad/out/Default/obj/third_party/mini_chromium/mini_chromium/base"

./waf configure --enable-debug
