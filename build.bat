@echo off
del *.pdb > NUL 2> NUL
del *.rdi > NUL 2> NUL
cl fedit.c -nologo -Fe:fedit.exe -Z7 -W4 -link -incremental:no -opt:ref
del *.ilk > NUL 2> NUL
del *.obj > NUL 2> NUL
