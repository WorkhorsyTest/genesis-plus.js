/* unzip.h -- IO for uncompress .zip files using zlib 
   Version 0.15 beta, Mar 19th, 1998,

   Copyright (C) 1998 Gilles Vollant

   This unzip package allow extract file from .ZIP file, compatible with PKZip 2.04g
     WinZip, InfoZip tools and compatible.
   Encryption and multi volume ZipFile (span) are not supported.
   Old compressions used by old PKZip 1.x are not supported

   THIS IS AN ALPHA VERSION. AT THIS STAGE OF DEVELOPPEMENT, SOMES API OR STRUCTURE
   CAN CHANGE IN FUTURE VERSION !!
   I WAIT FEEDBACK at mail info@winimage.com
   Visit also http://www.winimage.com/zLibDll/unzip.htm for evolution

   Condition of use and distribution are the same than zlib :

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* for more info about .ZIP format, see
      ftp://ftp.cdrom.com/pub/infozip/doc/appnote-970311-iz.zip
   PkWare has also a specification at :
      ftp://ftp.pkware.com/probdesc.zip */


import stdio;
import stdlib;
import zlib;
import types;
import unzip;
import errno;

version(STRICTUNZIP) {
/* like the STRICT of WIN32, we define a pointer that cannot be converted
    from (void*) without cast */
struct unzFile__ {
  int unused;
}
alias *unzFile unzFile__;
} else {
alias unzFile voidp;
}


const int UNZ_OK                  = 0;
const int UNZ_END_OF_LIST_OF_FILE = -100;
const int UNZ_ERRNO               = Z_ERRNO;
const int UNZ_EOF                 = 0;
const int UNZ_PARAMERROR          = -102;
const int UNZ_BADZIPFILE          = -103;
const int UNZ_INTERNALERROR       = -104;
const int UNZ_CRCERROR            = -105;

/* tm_unz contain date/time info */
struct tm_unz
{
  u32 tm_sec;            /* seconds after the minute - [0,59] */
  u32 tm_min;            /* minutes after the hour - [0,59] */
  u32 tm_hour;           /* hours since midnight - [0,23] */
  u32 tm_mday;           /* day of the month - [1,31] */
  u32 tm_mon;            /* months since January - [0,11] */
  u32 tm_year;           /* years - [1980..2044] */
}

/* unz_global_info structure contain global data about the ZIPfile
   These data comes from the end of central dir */
struct unz_global_info
{
  u32 number_entry; /* total number of entries in
                          the central dir on this disk */
  u32 size_comment; /* size of the global comment of the zipfile */
}


/* unz_file_info contain information about a file in the zipfile */
struct unz_file_info
{
    u32 version_made_by;      /* version made by                 2 bytes */
    u32 version_needed;       /* version needed to extract       2 bytes */
    u32 flag;                 /* general purpose bit flag        2 bytes */
    u32 compression_method;   /* compression method              2 bytes */
    u32 dosDate;              /* last mod file date in Dos fmt   4 bytes */
    u32 crc;                  /* crc-32                          4 bytes */
    u32 compressed_size;      /* compressed size                 4 bytes */ 
    u32 uncompressed_size;    /* uncompressed size               4 bytes */ 
    u32 size_filename;        /* filename length                 2 bytes */
    u32 size_file_extra;      /* extra field length              2 bytes */
    u32 size_file_comment;    /* file comment length             2 bytes */

    u32 disk_num_start;       /* disk number start               2 bytes */
    u32 internal_fa;          /* internal file attributes        2 bytes */
    u32 external_fa;          /* external file attributes        4 bytes */

    tm_unz tmu_date;
}



const int UNZ_BUFSIZE = 16384;
const int UNZ_MAXFILENAMEINZIP = 256;

void* ALLOC(size_t size) { return malloc(size); }
void TRYFREE(void* p) { if(p) free(p); }

const int SIZECENTRALDIRITEM = 0x2e;
const int SIZEZIPLOCALHEADER = 0x1e;

/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined.... */

const int SEEK_CUR    = 1;
const int SEEK_END    = 2;
const int SEEK_SET    = 0;

const char[] unz_copyright =
   " unzip 0.15 Copyright 1998 Gilles Vollant ";

/* unz_file_info_interntal contain internal info about a file in zipfile*/
struct unz_file_info_internal
{
    u32 offset_curfile;/* relative offset of local header 4 bytes */
}


/* file_in_zip_read_info_s contain internal information about a file in zipfile,
    when reading and decompress it */
struct file_in_zip_read_info_s
{
  char*  read_buffer;         /* internal buffer for compressed data */
  z_stream stream;            /* zLib stream structure for inflate */

  u32 pos_in_zipfile;       /* position in byte on the zipfile, for fseek*/
  u32 stream_initialised;   /* flag set if stream structure is initialised*/

  u32 offset_local_extrafield;/* offset of the local extra field */
  u32  size_local_extrafield;/* size of the local extra field */
  u32 pos_local_extrafield;   /* position in the local extra field in read*/

  u32 crc32;                /* crc32 of all data uncompressed */
  u32 crc32_wait;           /* crc32 we must obtain after decompress all */
  u32 rest_read_compressed; /* number of byte to be decompressed */
  u32 rest_read_uncompressed;/*number of byte to be obtained after decomp*/
  FILE* file;                 /* io structore of the zipfile */
  u32 compression_method;   /* compression method (0==store) */
  u32 byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
}


/* unz_s contain internal information about the zipfile
*/
struct unz_s
{
  FILE* file;                     /* io structore of the zipfile */
  unz_global_info gi;             /* public global information */
  u32 byte_before_the_zipfile;  /* byte before the zipfile, (>0 for sfx)*/
  u32 num_file;                 /* number of the current file in the zipfile*/
  u32 pos_in_central_dir;       /* pos of the current file in the central dir*/
  u32 current_file_ok;          /* flag about the usability of the current file*/
  u32 central_pos;              /* position of the beginning of the central dir*/

  u32 size_central_dir;         /* size of the central directory  */
  u32 offset_central_dir;       /* offset of start of central directory with
                                     respect to the starting disk number */

  unz_file_info cur_file_info;                    /* public info about the current file in zip*/
  unz_file_info_internal cur_file_info_internal;  /* private info about it*/
    file_in_zip_read_info_s* pfile_in_zip_read;   /* structure about the current
                                                      file if we are decompressing it */
}


/* ===========================================================================
  Read a byte from a gz_stream; update next_in and avail_in. Return EOF
  for end of file.
  IN assertion: the stream s has been sucessfully opened for reading.
*/


static  int unzlocal_getByte(fin,pi)
  FILE *fin;
  int *pi;
{
  u8 c;
  int err = fread(&c, 1, 1, fin);
  if (err==1)
  {
    *pi = (int)c;
    return UNZ_OK;
  }
  else
  {
    if (ferror(fin)) 
      return UNZ_ERRNO;
    else
      return UNZ_EOF;
  }
}


/* ===========================================================================
   Reads a long in LSB order from the given gz_stream. Sets 
*/
static  int unzlocal_getShort (fin,pX)
  FILE* fin;
  u32 *pX;
{
  u32 x ;
  int i = 0;
  int err;

  err = unzlocal_getByte(fin,&i);
  x = (u32)i;

  if (err==UNZ_OK)
    err = unzlocal_getByte(fin,&i);
  x += ((u32)i)<<8;

  if (err==UNZ_OK)
    *pX = x;
  else
    *pX = 0;
  return err;
}

static  int unzlocal_getLong (fin,pX)
  FILE* fin;
  u32 *pX;
{
  u32 x ;
  int i = 0;
  int err;

  err = unzlocal_getByte(fin,&i);
  x = (u32)i;

  if (err==UNZ_OK)
    err = unzlocal_getByte(fin,&i);
  x += ((u32)i)<<8;

  if (err==UNZ_OK)
    err = unzlocal_getByte(fin,&i);
  x += ((u32)i)<<16;

  if (err==UNZ_OK)
    err = unzlocal_getByte(fin,&i);
  x += ((u32)i)<<24;

  if (err==UNZ_OK)
    *pX = x;
  else
    *pX = 0;
  return err;
}


/* My own strcmpi / strcasecmp */
static  int strcmpcasenosensitive_internal (fileName1,fileName2)
  const char* fileName1;
  const char* fileName2;
{
  for (;;)
  {
    char c1=*(fileName1++);
    char c2=*(fileName2++);
    if ((c1>='a') && (c1<='z'))
      c1 -= 0x20;
    if ((c2>='a') && (c2<='z'))
      c2 -= 0x20;
    if (c1=='\0')
      return ((c2=='\0') ? 0 : -1);
    if (c2=='\0')
      return 1;
    if (c1<c2)
      return -1;
    if (c1>c2)
      return 1;
  }
}

const int CASESENSITIVITYDEFAULTVALUE = 2;

/* 
   Compare two filename (fileName1,fileName2).
   If iCaseSenisivity = 1, comparision is case sensitivity (like strcmp)
   If iCaseSenisivity = 2, comparision is not case sensitivity (like strcmpi
                                                                or strcasecmp)
   If iCaseSenisivity = 0, case sensitivity is defaut of your operating system
        (like 1 on Unix, 2 on Windows)

*/
extern int ZEXPORT unzStringFileNameCompare (fileName1,fileName2,iCaseSensitivity)
  const char* fileName1;
  const char* fileName2;
  int iCaseSensitivity;
{
  if (iCaseSensitivity==0)
    iCaseSensitivity=CASESENSITIVITYDEFAULTVALUE;

  if (iCaseSensitivity==1)
    return strcmp(fileName1,fileName2);

  return strcmpcasenosensitive_internal(fileName1,fileName2);
}

const int BUFREADCOMMENT = 0x400;

/*
  Locate the Central directory of a zipfile (at the end, just before
    the global comment)
*/
static  u32 unzlocal_SearchCentralDir(fin)
  FILE *fin;
{
  u8* buf;
  u32 uSizeFile;
  u32 uBackRead;
  u32 uMaxBack=0xffff; /* maximum size of global comment */
  u32 uPosFound=0;

  if (fseek(fin,0,SEEK_END) != 0)
    return 0;


  uSizeFile = ftell( fin );

  if (uMaxBack>uSizeFile)
    uMaxBack = uSizeFile;

  buf = (u8*)ALLOC(BUFREADCOMMENT+4);
  if (buf==NULL)
    return 0;

  uBackRead = 4;
  while (uBackRead<uMaxBack)
  {
    u32 uReadSize,uReadPos ;
    int i;
    if (uBackRead+BUFREADCOMMENT>uMaxBack) 
      uBackRead = uMaxBack;
    else
      uBackRead+=BUFREADCOMMENT;
    uReadPos = uSizeFile-uBackRead ;

    uReadSize = ((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ? 
                  (BUFREADCOMMENT+4) : (uSizeFile-uReadPos);
    if (fseek(fin,uReadPos,SEEK_SET)!=0)
      break;

    if (fread(buf,(u32)uReadSize,1,fin)!=1)
      break;

    for (i=(int)uReadSize-3; (i--)>0;)
      if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) && 
        ((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06))
      {
        uPosFound = uReadPos+i;
        break;
      }

    if (uPosFound!=0)
      break;
  }
  TRYFREE(buf);
  return uPosFound;
}

/*
  Open a Zip file. path contain the full pathname (by example,
  on a Windows NT computer "c:\\test\\zlib109.zip" or on an Unix computer
  "zlib/zlib109.zip".
  If the zipfile cannot be opened (file don't exist or in not valid), the
  return value is NULL.
  Else, the return value is a unzFile Handle, usable with other function
  of this unzip package.
*/
extern unzFile ZEXPORT unzOpen (path)
  const char *path;
{
  unz_s us;
  unz_s *s;
  u32 central_pos,uL;
  FILE * fin ;

  u32 number_disk;          /* number of the current dist, used for 
                   spaning ZIP, unsupported, always 0*/
  u32 number_disk_with_CD;  /* number the the disk with central dir, used
                   for spaning ZIP, unsupported, always 0*/
  u32 number_entry_CD;      /* total number of entries in
                                 the central dir 
                                 (same than number_entry on nospan) */

  int err=UNZ_OK;

  if (unz_copyright[0]!=' ')
    return NULL;

  fin=fopen(path,"rb");
  if (fin==NULL)
    return NULL;

  central_pos = unzlocal_SearchCentralDir(fin);
  if (central_pos==0)
    err=UNZ_ERRNO;

  if (fseek(fin,central_pos,SEEK_SET)!=0)
    err=UNZ_ERRNO;

  /* the signature, already checked */
  if (unzlocal_getLong(fin,&uL)!=UNZ_OK)
    err=UNZ_ERRNO;

  /* number of this disk */
  if (unzlocal_getShort(fin,&number_disk)!=UNZ_OK)
    err=UNZ_ERRNO;

  /* number of the disk with the start of the central directory */
  if (unzlocal_getShort(fin,&number_disk_with_CD)!=UNZ_OK)
    err=UNZ_ERRNO;

  /* total number of entries in the central dir on this disk */
  if (unzlocal_getShort(fin,&us.gi.number_entry)!=UNZ_OK)
    err=UNZ_ERRNO;

  /* total number of entries in the central dir */
  if (unzlocal_getShort(fin,&number_entry_CD)!=UNZ_OK)
    err=UNZ_ERRNO;

  if ((number_entry_CD!=us.gi.number_entry) ||
    (number_disk_with_CD!=0) ||
    (number_disk!=0))
    err=UNZ_BADZIPFILE;

  /* size of the central directory */
  if (unzlocal_getLong(fin,&us.size_central_dir)!=UNZ_OK)
    err=UNZ_ERRNO;

  /* offset of start of central directory with respect to the 
    starting disk number */
  if (unzlocal_getLong(fin,&us.offset_central_dir)!=UNZ_OK)
    err=UNZ_ERRNO;

  /* zipfile comment length */
  if (unzlocal_getShort(fin,&us.gi.size_comment)!=UNZ_OK)
    err=UNZ_ERRNO;

  if ((central_pos<us.offset_central_dir+us.size_central_dir) && 
    (err==UNZ_OK))
    err=UNZ_BADZIPFILE;

  if (err!=UNZ_OK)
  {
    fclose(fin);
    return NULL;
  }

  us.file=fin;
  us.byte_before_the_zipfile = central_pos -
                        (us.offset_central_dir+us.size_central_dir);
  us.central_pos = central_pos;
    us.pfile_in_zip_read = NULL;

  s=(unz_s*)ALLOC(sizeof(unz_s));
  *s=us;
  unzGoToFirstFile((unzFile)s);  
  return (unzFile)s;  
}


/*
  Close a ZipFile opened with unzipOpen.
  If there is files inside the .Zip opened with unzipOpenCurrentFile (see later),
  these files MUST be closed with unzipCloseCurrentFile before call unzipClose.
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzClose (file)
  unzFile file;
{
  unz_s* s;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;

  if (s->pfile_in_zip_read!=NULL)
    unzCloseCurrentFile(file);

  fclose(s->file);
  TRYFREE(s);
  return UNZ_OK;
}


/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzGetGlobalInfo (file,pglobal_info)
  unzFile file;
  unz_global_info *pglobal_info;
{
  unz_s* s;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  *pglobal_info=s->gi;
  return UNZ_OK;
}


/*
   Translate date/time from Dos format to tm_unz (readable more easilty)
*/
static  void unzlocal_DosDateToTmuDate (ulDosDate, ptm)
  u32 ulDosDate;
  tm_unz* ptm;
{
  u32 uDate;
  uDate = (u32)(ulDosDate>>16);
  ptm->tm_mday = (u32)(uDate&0x1f) ;
  ptm->tm_mon =  (u32)((((uDate)&0x1E0)/0x20)-1) ;
  ptm->tm_year = (u32)(((uDate&0x0FE00)/0x0200)+1980) ;

  ptm->tm_hour = (u32) ((ulDosDate &0xF800)/0x800);
  ptm->tm_min =  (u32) ((ulDosDate&0x7E0)/0x20) ;
  ptm->tm_sec =  (u32) (2*(ulDosDate&0x1f)) ;
}

/*
  Get Info about the current file in the zipfile, with internal only info
*/
static  int unzlocal_GetCurrentFileInfoInternal OF((unzFile file,
                                                  unz_file_info *pfile_info,
                                                  unz_file_info_internal 
                                                  *pfile_info_internal,
                                                  char *szFileName,
                          u32 fileNameBufferSize,
                                                  void *extraField,
                          u32 extraFieldBufferSize,
                                                  char *szComment,
                          u32 commentBufferSize));

static  int unzlocal_GetCurrentFileInfoInternal (file,
                                              pfile_info,
                                              pfile_info_internal,
                                              szFileName, fileNameBufferSize,
                                              extraField, extraFieldBufferSize,
                                              szComment,  commentBufferSize)
  unzFile file;
  unz_file_info *pfile_info;
  unz_file_info_internal *pfile_info_internal;
  char *szFileName;
  u32 fileNameBufferSize;
  void *extraField;
  u32 extraFieldBufferSize;
  char *szComment;
  u32 commentBufferSize;
{
  unz_s* s;
  unz_file_info file_info;
  unz_file_info_internal file_info_internal;
  int err=UNZ_OK;
  u32 uMagic;
  long lSeek=0;

  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  if (fseek(s->file,s->pos_in_central_dir+s->byte_before_the_zipfile,SEEK_SET)!=0)
    err=UNZ_ERRNO;


  /* we check the magic */
  if (err==UNZ_OK)
  {
    if (unzlocal_getLong(s->file,&uMagic) != UNZ_OK)
      err=UNZ_ERRNO;
    else if (uMagic!=0x02014b50)
      err=UNZ_BADZIPFILE;
  }

  if (unzlocal_getShort(s->file,&file_info.version_made_by) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.version_needed) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.flag) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.compression_method) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getLong(s->file,&file_info.dosDate) != UNZ_OK)
    err=UNZ_ERRNO;

  unzlocal_DosDateToTmuDate(file_info.dosDate,&file_info.tmu_date);

  if (unzlocal_getLong(s->file,&file_info.crc) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getLong(s->file,&file_info.compressed_size) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getLong(s->file,&file_info.uncompressed_size) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.size_filename) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.size_file_extra) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.size_file_comment) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.disk_num_start) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&file_info.internal_fa) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getLong(s->file,&file_info.external_fa) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getLong(s->file,&file_info_internal.offset_curfile) != UNZ_OK)
    err=UNZ_ERRNO;

  lSeek+=file_info.size_filename;
  if ((err==UNZ_OK) && (szFileName!=NULL))
  {
    u32 uSizeRead ;
    if (file_info.size_filename<fileNameBufferSize)
    {
      *(szFileName+file_info.size_filename)='\0';
      uSizeRead = file_info.size_filename;
    }
    else
      uSizeRead = fileNameBufferSize;

    if ((file_info.size_filename>0) && (fileNameBufferSize>0))
      if (fread(szFileName,(u32)uSizeRead,1,s->file)!=1)
        err=UNZ_ERRNO;
    lSeek -= uSizeRead;
  }

  if ((err==UNZ_OK) && (extraField!=NULL))
  {
    u32 uSizeRead ;
    if (file_info.size_file_extra<extraFieldBufferSize)
      uSizeRead = file_info.size_file_extra;
    else
      uSizeRead = extraFieldBufferSize;

    if (lSeek!=0)
    {
      if (fseek(s->file,lSeek,SEEK_CUR)==0)
        lSeek=0;
      else
        err=UNZ_ERRNO;
    }

    if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0))
      if (fread(extraField,(u32)uSizeRead,1,s->file)!=1)
        err=UNZ_ERRNO;
    lSeek += file_info.size_file_extra - uSizeRead;
  }
  else
    lSeek+=file_info.size_file_extra; 

  if ((err==UNZ_OK) && (szComment!=NULL))
  {
    u32 uSizeRead ;
    if (file_info.size_file_comment<commentBufferSize)
    {
      *(szComment+file_info.size_file_comment)='\0';
      uSizeRead = file_info.size_file_comment;
    }
    else
      uSizeRead = commentBufferSize;

    if (lSeek!=0)
    {
      if (fseek(s->file,lSeek,SEEK_CUR)==0)
        lSeek=0;
      else
        err=UNZ_ERRNO;
    }

    if ((file_info.size_file_comment>0) && (commentBufferSize>0))
      if (fread(szComment,(u32)uSizeRead,1,s->file)!=1)
        err=UNZ_ERRNO;
    lSeek+=file_info.size_file_comment - uSizeRead;
  }
  else
    lSeek+=file_info.size_file_comment;

  if ((err==UNZ_OK) && (pfile_info!=NULL))
    *pfile_info=file_info;

  if ((err==UNZ_OK) && (pfile_info_internal!=NULL))
    *pfile_info_internal=file_info_internal;

  return err;
}



/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem.
*/
extern int ZEXPORT unzGetCurrentFileInfo (file,
                                                pfile_info,
                                                szFileName, fileNameBufferSize,
                                                extraField, extraFieldBufferSize,
                                                szComment,  commentBufferSize)
  unzFile file;
  unz_file_info *pfile_info;
  char *szFileName;
  u32 fileNameBufferSize;
  void *extraField;
  u32 extraFieldBufferSize;
  char *szComment;
  u32 commentBufferSize;
{
  return unzlocal_GetCurrentFileInfoInternal(file,pfile_info,NULL,
                        szFileName,fileNameBufferSize,
                        extraField,extraFieldBufferSize,
                        szComment,commentBufferSize);
}

/*
  Set the current file of the zipfile to the first file.
  return UNZ_OK if there is no problem
*/
extern int ZEXPORT unzGoToFirstFile (file)
  unzFile file;
{
  int err=UNZ_OK;
  unz_s* s;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  s->pos_in_central_dir=s->offset_central_dir;
  s->num_file=0;
  err=unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
                       &s->cur_file_info_internal,
                       NULL,0,NULL,0,NULL,0);
  s->current_file_ok = (err == UNZ_OK);
  return err;
}


/*
  Set the current file of the zipfile to the next file.
  return UNZ_OK if there is no problem
  return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest.
*/
extern int ZEXPORT unzGoToNextFile (file)
  unzFile file;
{
  unz_s* s;  
  int err;

  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  if (!s->current_file_ok)
    return UNZ_END_OF_LIST_OF_FILE;
  if (s->num_file+1==s->gi.number_entry)
    return UNZ_END_OF_LIST_OF_FILE;

  s->pos_in_central_dir += SIZECENTRALDIRITEM + s->cur_file_info.size_filename +
      s->cur_file_info.size_file_extra + s->cur_file_info.size_file_comment ;
  s->num_file++;
  err = unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
                         &s->cur_file_info_internal,
                         NULL,0,NULL,0,NULL,0);
  s->current_file_ok = (err == UNZ_OK);
  return err;
}


/*
  Try locate the file szFileName in the zipfile.
  For the iCaseSensitivity signification, see unzipStringFileNameCompare

  return value :
  UNZ_OK if the file is found. It becomes the current file.
  UNZ_END_OF_LIST_OF_FILE if the file is not found
*/
extern int ZEXPORT unzLocateFile (file, szFileName, iCaseSensitivity)
  unzFile file;
  const char *szFileName;
  int iCaseSensitivity;
{
  unz_s* s;  
  int err;

  u32 num_fileSaved;
  u32 pos_in_central_dirSaved;

  if (file==NULL)
    return UNZ_PARAMERROR;

  if (strlen(szFileName)>=UNZ_MAXFILENAMEINZIP)
    return UNZ_PARAMERROR;

  s=(unz_s*)file;
  if (!s->current_file_ok)
    return UNZ_END_OF_LIST_OF_FILE;

  num_fileSaved = s->num_file;
  pos_in_central_dirSaved = s->pos_in_central_dir;

  err = unzGoToFirstFile(file);

  while (err == UNZ_OK)
  {
    char szCurrentFileName[UNZ_MAXFILENAMEINZIP+1];
    unzGetCurrentFileInfo(file,NULL,
                szCurrentFileName,sizeof(szCurrentFileName)-1,
                NULL,0,NULL,0);
    if (unzStringFileNameCompare(szCurrentFileName,
                    szFileName,iCaseSensitivity)==0)
      return UNZ_OK;
    err = unzGoToNextFile(file);
  }

  s->num_file = num_fileSaved ;
  s->pos_in_central_dir = pos_in_central_dirSaved ;
  return err;
}


/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
        directory about this file
  store in *piSizeVar the size of extra info in local header
        (filename and size of extra field data)
*/
static  int unzlocal_CheckCurrentFileCoherencyHeader (s,piSizeVar,
                          poffset_local_extrafield,
                          psize_local_extrafield)
  unz_s* s;
  u32* piSizeVar;
  u32 *poffset_local_extrafield;
  u32  *psize_local_extrafield;
{
  u32 uMagic,uData,uFlags;
  u32 size_filename;
  u32 size_extra_field;
  int err=UNZ_OK;

  *piSizeVar = 0;
  *poffset_local_extrafield = 0;
  *psize_local_extrafield = 0;

  if (fseek(s->file,s->cur_file_info_internal.offset_curfile +
                s->byte_before_the_zipfile,SEEK_SET)!=0)
    return UNZ_ERRNO;


  if (err==UNZ_OK)
  {
    if (unzlocal_getLong(s->file,&uMagic) != UNZ_OK)
      err=UNZ_ERRNO;
    else if (uMagic!=0x04034b50)
      err=UNZ_BADZIPFILE;
  }

  if (unzlocal_getShort(s->file,&uData) != UNZ_OK)
    err=UNZ_ERRNO;
/*
  else if ((err==UNZ_OK) && (uData!=s->cur_file_info.wVersion))
    err=UNZ_BADZIPFILE;
*/
  if (unzlocal_getShort(s->file,&uFlags) != UNZ_OK)
    err=UNZ_ERRNO;

  if (unzlocal_getShort(s->file,&uData) != UNZ_OK)
    err=UNZ_ERRNO;
  else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compression_method))
    err=UNZ_BADZIPFILE;

  if ((err==UNZ_OK) && (s->cur_file_info.compression_method!=0) &&
      (s->cur_file_info.compression_method!=Z_DEFLATED))
    err=UNZ_BADZIPFILE;

  if (unzlocal_getLong(s->file,&uData) != UNZ_OK) /* date/time */
    err=UNZ_ERRNO;

  if (unzlocal_getLong(s->file,&uData) != UNZ_OK) /* crc */
    err=UNZ_ERRNO;
  else if ((err==UNZ_OK) && (uData!=s->cur_file_info.crc) &&
            ((uFlags & 8)==0))
    err=UNZ_BADZIPFILE;

  if (unzlocal_getLong(s->file,&uData) != UNZ_OK) /* size compr */
    err=UNZ_ERRNO;
  else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compressed_size) &&
            ((uFlags & 8)==0))
    err=UNZ_BADZIPFILE;

  if (unzlocal_getLong(s->file,&uData) != UNZ_OK) /* size uncompr */
    err=UNZ_ERRNO;
  else if ((err==UNZ_OK) && (uData!=s->cur_file_info.uncompressed_size) && 
            ((uFlags & 8)==0))
    err=UNZ_BADZIPFILE;


  if (unzlocal_getShort(s->file,&size_filename) != UNZ_OK)
    err=UNZ_ERRNO;
  else if ((err==UNZ_OK) && (size_filename!=s->cur_file_info.size_filename))
    err=UNZ_BADZIPFILE;

  *piSizeVar += (u32)size_filename;

  if (unzlocal_getShort(s->file,&size_extra_field) != UNZ_OK)
    err=UNZ_ERRNO;
  *poffset_local_extrafield= s->cur_file_info_internal.offset_curfile +
                  SIZEZIPLOCALHEADER + size_filename;
  *psize_local_extrafield = (u32)size_extra_field;

  *piSizeVar += (u32)size_extra_field;

  return err;
}

/*
  Open for reading data the current file in the zipfile.
  If there is no error and the file is opened, the return value is UNZ_OK.
*/
extern int ZEXPORT unzOpenCurrentFile (file)
  unzFile file;
{
  int err=UNZ_OK;
  int Store;
  u32 iSizeVar;
  unz_s* s;
  file_in_zip_read_info_s* pfile_in_zip_read_info;
  u32 offset_local_extrafield;  /* offset of the local extra field */
  u32  size_local_extrafield;    /* size of the local extra field */

  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  if (!s->current_file_ok)
    return UNZ_PARAMERROR;

  if (s->pfile_in_zip_read != NULL)
    unzCloseCurrentFile(file);

  if (unzlocal_CheckCurrentFileCoherencyHeader(s,&iSizeVar,
        &offset_local_extrafield,&size_local_extrafield)!=UNZ_OK)
    return UNZ_BADZIPFILE;

  pfile_in_zip_read_info = (file_in_zip_read_info_s*)
                      ALLOC(sizeof(file_in_zip_read_info_s));
  if (pfile_in_zip_read_info==NULL)
    return UNZ_INTERNALERROR;

  pfile_in_zip_read_info->read_buffer=(char*)ALLOC(UNZ_BUFSIZE);
  pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
  pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
  pfile_in_zip_read_info->pos_local_extrafield=0;

  if (pfile_in_zip_read_info->read_buffer==NULL)
  {
    TRYFREE(pfile_in_zip_read_info);
    return UNZ_INTERNALERROR;
  }

  pfile_in_zip_read_info->stream_initialised=0;
  
  if ((s->cur_file_info.compression_method!=0) &&
        (s->cur_file_info.compression_method!=Z_DEFLATED))
    err=UNZ_BADZIPFILE;
  Store = s->cur_file_info.compression_method==0;

  pfile_in_zip_read_info->crc32_wait=s->cur_file_info.crc;
  pfile_in_zip_read_info->crc32=0;
  pfile_in_zip_read_info->compression_method =
            s->cur_file_info.compression_method;
  pfile_in_zip_read_info->file=s->file;
  pfile_in_zip_read_info->byte_before_the_zipfile=s->byte_before_the_zipfile;

  pfile_in_zip_read_info->stream.total_out = 0;

  if (!Store)
  {
    pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
    pfile_in_zip_read_info->stream.zfree = (free_func)0;
    pfile_in_zip_read_info->stream.opaque = (voidpf)0; 
      
    err=inflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
    if (err == Z_OK)
      pfile_in_zip_read_info->stream_initialised=1;
        /* windowBits is passed < 0 to tell that there is no zlib header.
         * Note that in this case inflate *requires* an extra "dummy" byte
         * after the compressed stream in order to complete decompression and
         * return Z_STREAM_END. 
         * In unzip, i don't wait absolutely Z_STREAM_END because I known the 
         * size of both compressed and uncompressed data
         */
  }
  pfile_in_zip_read_info->rest_read_compressed = 
            s->cur_file_info.compressed_size ;
  pfile_in_zip_read_info->rest_read_uncompressed = 
            s->cur_file_info.uncompressed_size ;

  pfile_in_zip_read_info->pos_in_zipfile = 
            s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + 
        iSizeVar;

  pfile_in_zip_read_info->stream.avail_in = (u32)0;


  s->pfile_in_zip_read = pfile_in_zip_read_info;
  return UNZ_OK;
}


/*
  Read bytes from the current file.
  buf contain buffer where data must be copied
  len the size of buf.

  return the number of byte copied if somes bytes are copied
  return 0 if the end of file was reached
  return <0 with error code if there is an error
    (UNZ_ERRNO for IO error, or zLib error for uncompress error)
*/
extern int ZEXPORT unzReadCurrentFile  (file, buf, len)
  unzFile file;
  voidp buf;
  u32 len;
{
  int err=UNZ_OK;
  u32 iRead = 0;
  unz_s* s;
  file_in_zip_read_info_s* pfile_in_zip_read_info;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  pfile_in_zip_read_info=s->pfile_in_zip_read;

  if (pfile_in_zip_read_info==NULL)
    return UNZ_PARAMERROR;

  if (pfile_in_zip_read_info->read_buffer == NULL)
    return UNZ_END_OF_LIST_OF_FILE;
  if (len==0)
    return 0;

  pfile_in_zip_read_info->stream.next_out = (Bytef*)buf;

  pfile_in_zip_read_info->stream.avail_out = (u32)len;

  if (len>pfile_in_zip_read_info->rest_read_uncompressed)
    pfile_in_zip_read_info->stream.avail_out = 
      (u32)pfile_in_zip_read_info->rest_read_uncompressed;

  while (pfile_in_zip_read_info->stream.avail_out>0)
  {
    if ((pfile_in_zip_read_info->stream.avail_in==0) &&
        (pfile_in_zip_read_info->rest_read_compressed>0))
    {
      u32 uReadThis = UNZ_BUFSIZE;
      if (pfile_in_zip_read_info->rest_read_compressed<uReadThis)
        uReadThis = (u32)pfile_in_zip_read_info->rest_read_compressed;
      if (uReadThis == 0)
        return UNZ_EOF;
      if (fseek(pfile_in_zip_read_info->file,
                      pfile_in_zip_read_info->pos_in_zipfile + 
                         pfile_in_zip_read_info->byte_before_the_zipfile,SEEK_SET)!=0)
        return UNZ_ERRNO;
      if (fread(pfile_in_zip_read_info->read_buffer,uReadThis,1,
                         pfile_in_zip_read_info->file)!=1)
        return UNZ_ERRNO;
      pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

      pfile_in_zip_read_info->rest_read_compressed-=uReadThis;

      pfile_in_zip_read_info->stream.next_in = 
                (Bytef*)pfile_in_zip_read_info->read_buffer;
      pfile_in_zip_read_info->stream.avail_in = (u32)uReadThis;
    }

    if (pfile_in_zip_read_info->compression_method==0)
    {
      u32 uDoCopy,i ;
      if (pfile_in_zip_read_info->stream.avail_out < 
                            pfile_in_zip_read_info->stream.avail_in)
        uDoCopy = pfile_in_zip_read_info->stream.avail_out ;
      else
        uDoCopy = pfile_in_zip_read_info->stream.avail_in ;

      for (i=0;i<uDoCopy;i++)
        *(pfile_in_zip_read_info->stream.next_out+i) =
                        *(pfile_in_zip_read_info->stream.next_in+i);

      pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32,
                pfile_in_zip_read_info->stream.next_out,
                uDoCopy);
      pfile_in_zip_read_info->rest_read_uncompressed-=uDoCopy;
      pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
      pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
      pfile_in_zip_read_info->stream.next_out += uDoCopy;
      pfile_in_zip_read_info->stream.next_in += uDoCopy;
      pfile_in_zip_read_info->stream.total_out += uDoCopy;
      iRead += uDoCopy;
    }
    else
    {
      u32 uTotalOutBefore,uTotalOutAfter;
      const Bytef *bufBefore;
      u32 uOutThis;
      int flush=Z_SYNC_FLUSH;

      uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
      bufBefore = pfile_in_zip_read_info->stream.next_out;

      /*
      if ((pfile_in_zip_read_info->rest_read_uncompressed ==
               pfile_in_zip_read_info->stream.avail_out) &&
        (pfile_in_zip_read_info->rest_read_compressed == 0))
        flush = Z_FINISH;
      */
      err=inflate(&pfile_in_zip_read_info->stream,flush);

      uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
      uOutThis = uTotalOutAfter-uTotalOutBefore;

      pfile_in_zip_read_info->crc32 = 
                crc32(pfile_in_zip_read_info->crc32,bufBefore,
                        (u32)(uOutThis));

      pfile_in_zip_read_info->rest_read_uncompressed -=
                uOutThis;

      iRead += (u32)(uTotalOutAfter - uTotalOutBefore);

      if (err==Z_STREAM_END)
        return (iRead==0) ? UNZ_EOF : iRead;
      if (err!=Z_OK) 
        break;
    }
  }

  if (err==Z_OK)
    return iRead;
  return err;
}


/*
  Give the current position in uncompressed data
*/
extern z_off_t ZEXPORT unztell (file)
  unzFile file;
{
  unz_s* s;
  file_in_zip_read_info_s* pfile_in_zip_read_info;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  pfile_in_zip_read_info=s->pfile_in_zip_read;

  if (pfile_in_zip_read_info==NULL)
    return UNZ_PARAMERROR;

  return (z_off_t)pfile_in_zip_read_info->stream.total_out;
}


/*
  return 1 if the end of file was reached, 0 elsewhere 
*/
extern int ZEXPORT unzeof (file)
  unzFile file;
{
  unz_s* s;
  file_in_zip_read_info_s* pfile_in_zip_read_info;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

  if (pfile_in_zip_read_info==NULL)
    return UNZ_PARAMERROR;

  if (pfile_in_zip_read_info->rest_read_uncompressed == 0)
    return 1;
  else
    return 0;
}

/*
  Read extra field from the current file (opened by unzOpenCurrentFile)
  This is the local-header version of the extra field (sometimes, there is
    more info in the local-header version than in the central-header)

  if buf==NULL, it return the size of the local extra field that can be read

  if buf!=NULL, len is the size of the buffer, the extra header is copied in
  buf.
  the return value is the number of bytes copied in buf, or (if <0) 
  the error code
*/
extern int ZEXPORT unzGetLocalExtrafield (file,buf,len)
  unzFile file;
  voidp buf;
  u32 len;
{
  unz_s* s;
  file_in_zip_read_info_s* pfile_in_zip_read_info;
  u32 read_now;
  u32 size_to_read;

  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

  if (pfile_in_zip_read_info==NULL)
    return UNZ_PARAMERROR;

  size_to_read = (pfile_in_zip_read_info->size_local_extrafield - 
        pfile_in_zip_read_info->pos_local_extrafield);

  if (buf==NULL)
    return (int)size_to_read;

  if (len>size_to_read)
    read_now = (u32)size_to_read;
  else
    read_now = (u32)len ;

  if (read_now==0)
    return 0;

  if (fseek(pfile_in_zip_read_info->file,
        pfile_in_zip_read_info->offset_local_extrafield + 
        pfile_in_zip_read_info->pos_local_extrafield,SEEK_SET)!=0)
    return UNZ_ERRNO;

  if (fread(buf,(u32)size_to_read,1,pfile_in_zip_read_info->file)!=1)
    return UNZ_ERRNO;

  return (int)read_now;
}

/*
  Close the file in zip opened with unzipOpenCurrentFile
  Return UNZ_CRCERROR if all the file was read but the CRC is not good
*/
extern int ZEXPORT unzCloseCurrentFile (file)
  unzFile file;
{
  int err=UNZ_OK;

  unz_s* s;
  file_in_zip_read_info_s* pfile_in_zip_read_info;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;
  pfile_in_zip_read_info=s->pfile_in_zip_read;

  if (pfile_in_zip_read_info==NULL)
    return UNZ_PARAMERROR;


  if (pfile_in_zip_read_info->rest_read_uncompressed == 0)
  {
    if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait)
      err=UNZ_CRCERROR;
  }


  TRYFREE(pfile_in_zip_read_info->read_buffer);
  pfile_in_zip_read_info->read_buffer = NULL;
  if (pfile_in_zip_read_info->stream_initialised)
    inflateEnd(&pfile_in_zip_read_info->stream);

  pfile_in_zip_read_info->stream_initialised = 0;
  TRYFREE(pfile_in_zip_read_info);

  s->pfile_in_zip_read=NULL;

  return err;
}


/*
  Get the global comment string of the ZipFile, in the szComment buffer.
  uSizeBuf is the size of the szComment buffer.
  return the number of byte copied or an error code <0
*/
extern int ZEXPORT unzGetGlobalComment (file, szComment, uSizeBuf)
  unzFile file;
  char *szComment;
  u32 uSizeBuf;
{
/* int err=UNZ_OK; */
  unz_s* s;
  u32 uReadThis ;
  if (file==NULL)
    return UNZ_PARAMERROR;
  s=(unz_s*)file;

  uReadThis = uSizeBuf;
  if (uReadThis>s->gi.size_comment)
    uReadThis = s->gi.size_comment;

  if (fseek(s->file,s->central_pos+22,SEEK_SET)!=0)
    return UNZ_ERRNO;

  if (uReadThis>0)
  {
    *szComment='\0';
    if (fread(szComment,(u32)uReadThis,1,s->file)!=1)
    return UNZ_ERRNO;
  }

  if ((szComment != NULL) && (uSizeBuf > s->gi.size_comment))
    *(szComment+s->gi.size_comment)='\0';
  return (int)uReadThis;
}
