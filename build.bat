@echo off
del *.pdb > NUL 2> NUL
del *.rdi > NUL 2> NUL
cl src/fedit.c -nologo -Fe:fedit.exe -Z7 -W4 -external:anglebrackets -external:W0 -D_CRT_SECURE_NO_WARNINGS -wd4063 -link -incremental:no -opt:ref Ws2_32.lib
del *.ilk > NUL 2> NUL
del *.obj > NUL 2> NUL
