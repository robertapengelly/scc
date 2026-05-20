#******************************************************************************
# @file             Makefile.wat
#******************************************************************************
SRCDIR              ?=  $(CURDIR)
VPATH               :=  $(SRCDIR)

CPP_DIR             :=  $(SRCDIR)/cpp
CC1_DIR             :=  $(SRCDIR)/cc1

CPP_SRC             :=  $(wildcard $(CPP_DIR)/*.c)
CC1_SRC             :=  $(wildcard $(CC1_DIR)/*.c)

all: scc-cpp.exe scc-cc1.exe

scc-cpp.exe: $(CPP_SRC)

	wcl -fe=$@ $(subst /,\,$^)

scc-cc1.exe: $(CC1_SRC)

	wcl -fe=$@ $(subst /,\,$^)

clean:

	for /r %%f in (*.obj) do ( if exist %%f ( del /q %%f ) )

	if exist scc-cpp.exe ( del /q scc-cpp.exe )
	if exist scc-cc1.exe ( del /q scc-cc1.exe )

	if exist scc-cpp ( del /q scc-cpp )
	if exist scc-cc1 ( del /q scc-cc1 )
