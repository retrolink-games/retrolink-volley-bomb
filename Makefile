# Link Volley - Butano project Makefile (based on the Butano template)
TARGET      	:=  link-volley
BUILD       	:=  build
LIBBUTANO   	:=  lib/butano/butano
PYTHON      	:=  python3
SOURCES     	:=  src src/sim src/net src/render src/scenes src/sound
INCLUDES    	:=  include src lib/gba-link-connection/lib
DATA        	:=
GRAPHICS    	:=  graphics
AUDIO       	:=  audio
AUDIOBACKEND	:=  maxmod
AUDIOTOOL		:=
DMGAUDIO    	:=  dmg_audio
DMGAUDIOBACKEND	:=  default
ROMTITLE    	:=  VOLLEY BOMB
ROMCODE     	:=  RLVB
USERFLAGS   	:=  -DLINK_CABLE_QUEUE_SIZE=30 $(EXTRAFLAGS)
USERCXXFLAGS	:=
USERASFLAGS 	:=
USERLDFLAGS 	:=
USERLIBDIRS 	:=
USERLIBS    	:=
DEFAULTLIBS 	:=
STACKTRACE		:=
USERBUILD   	:=
EXTTOOL     	:=

ifndef LIBBUTANOABS
	export LIBBUTANOABS	:=	$(realpath $(LIBBUTANO))
endif

include $(LIBBUTANOABS)/butano.mak
