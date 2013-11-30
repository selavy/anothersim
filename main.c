#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_STR 256
#define NUMREGS 32
#define MEMSZ 125
#define WORD 8
#define MAXINST 128
#define NOTUSED -1

#define MEMLOC( x ) ( x / WORD )

#define _DEBUG

/* TYPES */
typedef int32_t RegVal;
typedef int32_t MemVal;

enum inst_t
  {
    DADD,
    SUB,
    LD,
    SD,
    BNEZ
  };

struct inst
{
  enum inst_t itype;
  char label[MAX_STR];
  size_t labelsz;
  int rs;
  int rt;
  int rd;
  char btarget[MAX_STR];
  size_t btargetsz;
  int32_t value;
};

struct inst * Instructions[MAXINST];
RegVal Registers[NUMREGS];
MemVal Memory[MEMSZ];
int instcount = 0;

/* FUNCTIONS */
void print_regs()
{
  int i;
  printf("REGISTERS\n");
  for( i = 0; i < NUMREGS; ++i )
    {
      if( Registers[i] != 0 )
	printf("R%d %d\n", i, Registers[i] );
    }
}

void print_mem()
{
  int i;
  printf("MEMORY\n");
  for( i = 0; i < MEMSZ; ++i )
    {
      if( Memory[i] != 0 )
	printf("%d %d\n", i * WORD, Memory[i] );
    }
}

void print_insts()
{
  int i;
  struct inst * curr;
  printf("INSTS\n");
  for( i = 0; i < instcount; ++i )
    {
      printf("------------------\n");
      curr = Instructions[i];
      printf("type: %d\n", (*curr).itype );

      if( (*curr).labelsz > 0 )
	printf("label: %s\n", (*curr).label );

      if( (*curr).rs != NOTUSED )
	printf("rs: %d\n", (*curr).rs );
      
      if( (*curr).rt != NOTUSED )
	printf("rt: %d\n", (*curr).rt );
      
      if( (*curr).rd != NOTUSED )
	printf("rd: %d\n", (*curr).rd );

      if( (*curr).btargetsz > 0 )
	printf("branch target: %s\n", (*curr).btarget );
      
      if( (*curr).value != NOTUSED )
	printf("value: %d\n", (*curr).value );

      printf("-------------------\n");
    }
}

int read_regs( FILE * pFile )
{
  char line[MAX_STR+1];
  int reg;
  RegVal value;

  if( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( strncmp( line, "REGISTERS", 9 ) != 0 )
	return 1;
    }
  else
    return 1;

  /* Read the Registers Section */
  while( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( strncmp( line, "MEMORY", 6 ) == 0 )
	break;

      if( sscanf( line, "R%d %d\n", &reg, &value ) != 2 )
	continue;
      
      if( ( reg >= 0 ) && ( reg < NUMREGS ) )
	Registers[reg] = value;
    }
  return 0;
}

void init()
{
  int i;
 /* init the registers and memory file */
  for( i = 0; i < NUMREGS; ++i )
    Registers[i] = 0;
  for( i = 0; i < MEMSZ; ++i )
    Memory[i] = 0;
  for( i = 0; i < MAXINST; ++i )
    Instructions[i] = NULL;
  instcount = 0;
}

int main( int argc, char ** argv )
{
  /* variables */
  const char * file_name = "input.txt";
  char line[MAX_STR+1];
  char * tmpline;
  char label[MAX_STR+1];
  char target[MAX_STR+1];
  int loc;
  int rd;
  int rs;
  int rt;
  int offset;
  RegVal value;
  struct inst * curr;

  /*---------------------------------------------------*/

  /* Open File */
  FILE * pFile = fopen( file_name, "r" );
  if( pFile == NULL )
    return 1;
  
  init();

  if( read_regs( pFile ) ) goto error;

  while( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( strncmp( line, "CODE", 4 ) == 0 )
	break;

      if( sscanf( line, "%d %d\n", &loc, &value ) != 2 )
	continue;
      
      loc = MEMLOC( loc );
      if( ( loc >= 0 ) && ( loc < MEMSZ ) )
	Memory[loc] = value;
    }

  while( fgets( line, MAX_STR, pFile ) != NULL )
    {
      if( Instructions[instcount] == NULL )
	{
	  Instructions[instcount] = malloc( sizeof( Instructions[instcount] ) );
	  if( Instructions[instcount] == NULL )
	    goto error;
	}

      curr = Instructions[instcount];

      if( ( tmpline = strstr( line, ":" ) ) != NULL )
	{
	  sscanf( line, "%s: %*s\n", label );
	  do { tmpline++; } while( isspace( *tmpline ) );	 
	  strcpy( line, tmpline );
	  label[ strlen(label) - 1 ] = '\0';
	  strcpy( (*curr).label, label );
	  (*curr).labelsz = strlen( label );
	}
      else
	{
	  strcpy( label, "" );
	  (*curr).label[0] = '\0';
	  (*curr).labelsz = 0;
	}

#ifdef _DEBUG
      printf("label => %s: ", label );
#endif

      if( strstr( line, "DADD" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, R%d, R%d\n", &rd, &rs, &rt ) != 3 )
	    {
	      if( sscanf( line, "%*s R%d, R%d, #%d\n", &rd, &rs, &value ) != 3 )
		continue;

	      rt = value; /* for the debug print line */
	      (*curr).rt = NOTUSED;
	      (*curr).value = rt;
	    }
	  else
	    {
	      (*curr).rt = rt;
	      (*curr).value = NOTUSED;
	    }

	  #ifdef _DEBUG
	  printf("DADD rd = %d, rs = %d, rt = %d\n", rd, rs, rt );
	  #endif

	  (*curr).itype = DADD;
	  (*curr).rs = rs;
	  (*curr).rd = rd;
	  (*curr).btarget[0] = '\0';
	  (*curr).btargetsz = 0;
	}
      else if( strstr( line, "SUB" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, R%d, R%d\n", &rd, &rs, &rt ) != 3 )
	    {
	      if( sscanf( line, "%*s R%d, R%d, #%d\n", &rd, &rs, &rt ) != 3 )
		continue;

	      rt = value; /* for the debug print line */
	      (*curr).rt = NOTUSED;
	      (*curr).value = rt;
	    }
	  else
	    {
	      (*curr).rt = rt;
	      (*curr).value = NOTUSED;
	    }
	  
	  #ifdef _DEBUG
	  printf("SUB rd = %d, rs = %d, rt = %d\n", rd, rs, rt );
	  #endif

	  (*curr).itype = SUB;
	  (*curr).rs = rs;
	  (*curr).rd = rd;
	  (*curr).btarget[0] = '\0';
	}
      else if( strstr( line, "LD" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, %d(R%d)\n", &rt, &offset, &rs ) != 3 ) continue;

	  #ifdef _DEBUG
	  printf("LD rt = %d, offset = %d, rs = %d\n", rt, offset, rs );
	  #endif

	  (*curr).itype = LD;
	  (*curr).rs = rs;
	  (*curr).rt = rt;
	  (*curr).rd = NOTUSED;
	  (*curr).btarget[0] = '\0';
	  (*curr).btargetsz = 0;
	  (*curr).value = offset;
	}
      else if( strstr( line, "SD" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, %d(R%d)\n", &rt, &offset, &rs ) != 3 ) continue;

	  #ifdef _DEBUG
	  printf("SD rt = %d, offset = %d, rs = %d\n", rt, offset, rs );
	  #endif
	  
	  (*curr).itype = SD;
	  (*curr).rs = rs;
	  (*curr).rt = rt;
	  (*curr).rd = NOTUSED;
	  (*curr).btarget[0] = '\0';
	  (*curr).btargetsz = 0;
	  (*curr).value = offset;
	}
      else if( strstr( line, "BNEZ" ) != NULL )
	{
	  if( sscanf( line, "%*s R%d, %s\n", &rs, target ) != 2 ) continue;

	  #ifdef _DEBUG
	  printf("BNEZ rs = %d, target = %s\n", rs, target );
	  #endif

	  (*curr).itype = BNEZ;
	  (*curr).rs = rs;
	  (*curr).rt = NOTUSED;
	  (*curr).rd = NOTUSED;
	  strcpy( (*curr).btarget, target );
	  (*curr).btargetsz = strlen( target );
	  (*curr).value = NOTUSED;
	}
      else
	{
          #ifdef _DEBUG
	  printf("command not recognized: %s\n", line );
	  #endif
	  continue;
	}

	instcount++;
    }

#ifdef _DEBUG
  print_insts();
#endif
  /* Pipeline */
  /* WB */
  /* MEM3 */
  /* MEM2 */
  /* MEM1 */
  /* EX */
  /* ID */
  /* IF2 */
  /* IF1 */
  /* end Pipeline */

  /* print end status */
  print_regs();
  print_mem();
  goto end;
 error:
  printf("ERROR Will Robinson!\n");
 end:
  fclose( pFile );
  return 0;
}
