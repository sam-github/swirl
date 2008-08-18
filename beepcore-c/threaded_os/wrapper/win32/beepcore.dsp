# Microsoft Developer Studio Project File - Name="beepcore" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=beepcore - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "beepcore.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "beepcore.mak" CFG="beepcore - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "beepcore - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "beepcore - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "beepcore - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BEEPCORE_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BEEPCORE_EXPORTS" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /dll /machine:I386

!ELSEIF  "$(CFG)" == "beepcore - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BEEPCORE_EXPORTS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D _WIN32_WINNT=0x500 /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BEEPCORE_EXPORTS" /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /dll /pdb:none /debug /debugtype:both /machine:I386
# Begin Custom Build - Creating dbg file
TargetDir=.\Debug
TargetPath=.\Debug\beepcore.dll
TargetName=beepcore
InputPath=.\Debug\beepcore.dll
SOURCE="$(InputPath)"

"$(TargetDir)\$(TargetName).dbg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	rebase -b 0x10000000 -x . $(TargetPath) 
	echo $(TargetDir)\$(TargetName).dbg 
	
# End Custom Build

!ENDIF 

# Begin Target

# Name "beepcore - Win32 Release"
# Name "beepcore - Win32 Debug"
# Begin Group "utility"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\utility\bp_config.c
# End Source File
# Begin Source File

SOURCE=..\..\utility\bp_config.h
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_hash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_hash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_malloc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_malloc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_queue.c
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_queue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_slist_utility.c
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\bp_slist_utility.h
# End Source File
# Begin Source File

SOURCE=..\..\utility\logconf.c
# End Source File
# Begin Source File

SOURCE=..\..\utility\logutil.c
# End Source File
# Begin Source File

SOURCE=..\..\utility\logutil.h
# End Source File
# Begin Source File

SOURCE=..\..\utility\workthread.c
# End Source File
# Begin Source File

SOURCE=..\..\utility\workthread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\xml_entities.c
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\xml_entities.h
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\xml_parse_constants.h
# End Source File
# Begin Source File

SOURCE=..\..\..\utility\xml_parse_globals.c
# End Source File
# End Group
# Begin Group "core"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\core\generic\base64.c
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\base64.h
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\CBEEP.c
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\CBEEP.h
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\CBEEPint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\cbxml_entities.c
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\cbxml_entities.h
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\channel_0.c
# End Source File
# Begin Source File

SOURCE=..\..\..\core\generic\channel_0.h
# End Source File
# End Group
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\beepcore.cpp
# End Source File
# Begin Source File

SOURCE=.\beepcore.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\beepcore.h
# End Source File
# End Group
# Begin Group "wrapper"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\bp_notify.c
# End Source File
# Begin Source File

SOURCE=..\bp_notify.h
# End Source File
# Begin Source File

SOURCE=..\bp_wrapper.c
# End Source File
# Begin Source File

SOURCE=..\bp_wrapper.h
# End Source File
# Begin Source File

SOURCE=..\bp_wrapper_private.h
# End Source File
# Begin Source File

SOURCE=..\bpc_wrapper.c
# End Source File
# Begin Source File

SOURCE=..\profile_loader.c
# End Source File
# Begin Source File

SOURCE=..\profile_loader.h
# End Source File
# End Group
# Begin Group "transport"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\transport\bp_fcbeeptcp.c
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_fcbeeptcp.h
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_fiostate.c
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_fiostate.h
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_fpoll.c
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_fpollmgr.c
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_fpollmgr.h
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_tcp.c
# End Source File
# Begin Source File

SOURCE=..\..\transport\bp_tcp.h
# End Source File
# End Group
# End Target
# End Project
