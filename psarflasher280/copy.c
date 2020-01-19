#include <pspkernel.h>
#include <pspdisplay.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <psppower.h>
#include <pspsdk.h>
#include <pspreg.h>
#include <psputilsforkernel.h>

PSP_MODULE_INFO("2.8downdater", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define MIPS_J(ADDR)          (0x08000000 + ((((unsigned int)(ADDR))&0x0ffffffc)>>2))

#define printf	pspDebugScreenPrintf

SceModule * (* sceKernelFindModuleByName_k)(const char *modname);
SceUID (* sceKernelLoadModule_k)(const char *path, int flags, SceKernelLMOption *option);
int (* sceKernelStartModule_k)(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);

int (* sceSysconResetDevice_k)(int, int)=NULL;

void asmKernelDcacheWritebackInvalidateAll(void)
{
   asm(".set    noreorder\n"
      "move      $t0,  $0\n"
      "addiu     $t1,  $0, 0x4000\n"
      "loop:\n"
      "cache     0x14, 0($t0)\n"
      "addiu     $t0,  $t0, 0x40\n"
      "bne       $t0,  $t1, loop\n"
      "cache     0x14, -0x40($t0)\n"
      "sync");
}

char debug[512];
char debug2[512];

void fillvram(int colour)
{
  long *lptr = (long*)0x44000000;

  while (lptr < (long*)0x44200000)
  {
    *lptr++ = colour;
  }
}

/* New FindProc based on tyranid's psplink code. PspPet one doesn't work
   well with 2.7X+ sysmem.prx */
u32 FindProc(const char* szMod, const char* szLib, u32 nid)
{
	SceModule * (* sceKernelFindModuleByName_k)(const char *modname) = (void *)0x0801c8a0;
//	SceModule * (* sceKernelFindModuleByName_k)(const char *modname) = (void *)0x88017624; // 1.50

	struct SceLibraryEntryTable *entry;
	SceModule *pMod;
	void *entTab;
	int entLen;

	pspSdkSetK1(0);
	pMod = sceKernelFindModuleByName_k (szMod);

	if (!pMod)
	{
		printf("Cannot find module %s\n", szMod);
		return 0;
	}
	
	int i = 0;

	entTab = pMod->ent_top;
	entLen = pMod->ent_size;
	// printf("entTab %p - entLen %d\n", entTab, entLen);
	while(i < entLen)
    {
		int count;
		int total;
		unsigned int *vars;

		entry = (struct SceLibraryEntryTable *) (entTab + i);

        if(entry->libname && !strcmp(entry->libname, szLib))
		{
			total = entry->stubcount + entry->vstubcount;
			vars = entry->entrytable;

			if(entry->stubcount > 0)
			{
				for(count = 0; count < entry->stubcount; count++)
				{
					if (vars[count] == nid)
						return vars[count+total];					
				}
			}
		}

		i += (entry->len * 4);
	}

	printf("Funtion not found.\n");
	return 0;
}

void ResetLogFile()
{
	SceUID fd = sceIoOpen("ms0:/downgrader.log", PSP_O_WRONLY | PSP_O_TRUNC | PSP_O_CREAT, 0777);
	sceIoClose(fd);
}

void OutputLogFile(char *str)
{
	SceUID fd = sceIoOpen("ms0:/downgrader.log", PSP_O_WRONLY | PSP_O_APPEND | PSP_O_CREAT, 0777);
	if (fd >= 0)
	{
  	sceIoWrite(fd, str, strlen(str));
  	sceIoClose(fd);
	}
}


void LOGSTR1(const char* str, unsigned long parm1)
{
  char buff[500];

	sprintf(buff, str, parm1);
	OutputLogFile(buff);
}

void LOGSTR2(const char* str, unsigned long parm1, unsigned long parm2)
{
  char buff[500];

	sprintf(buff, str, parm1, parm2);
	OutputLogFile(buff);
}


void DumpMyThreadInfo()
{
  int                 lrc;
  SceKernelThreadInfo ti;

  OutputLogFile("Info for current thread:\n");

  memset((void*)&ti, 0, sizeof(SceKernelThreadInfo));
  ti.size = sizeof(SceKernelThreadInfo);

  lrc= sceKernelReferThreadStatus(0, &ti);
  if (lrc>=0)
  {
    LOGSTR1("  Name:   '%s'\n", (unsigned long)(ti.name));
    LOGSTR2("  Size:   %08lX (expected: %08lX)\n", ti.size, sizeof(SceKernelThreadInfo));
    LOGSTR1("  Attr:   %08lX\n", ti.attr);
    LOGSTR1("  Status: %08lX\n", ti.status);
    LOGSTR1("  Entry:  %08lX\n", (unsigned long)(ti.entry));
    LOGSTR1("  Stack:  %08lX\n", (unsigned long)(ti.stack));
    LOGSTR1("  StkSiz: %08lX\n", ti.stackSize);
    LOGSTR1("  GP:     %08lX\n", (unsigned long)(ti.gpReg));
    LOGSTR1("  iniPri: %08lX\n", ti.initPriority);
    LOGSTR1("  curPri: %08lX\n", ti.currentPriority);
    LOGSTR1("  waitTy: %08lX\n", ti.waitType);
    LOGSTR1("  waitID: %08lX\n", ti.waitId);
    LOGSTR1("  wake #: %08lX\n", ti.wakeupCount);
    LOGSTR1("  exitSt: %08lX\n", ti.exitStatus);
  }
  else
  {
    LOGSTR1("ThreadStatus failed with %08lX\n", lrc);
  }
}


// this is the entry point for our genuine kernel thread.
void threadkproc(void)
{
	ResetLogFile();
	OutputLogFile("test logging\n");

	printf("Entered kernel mode...\n");
//	DumpMyThreadInfo();

	downgrader();

	for(;;)
		sceDisplayWaitVblankStart();
}

struct SceLibStubEntry{
	char *moduleName;
	unsigned short version;
	unsigned short attr;
	unsigned char structsz; // 0x5
	unsigned char numVars;
	unsigned short numFuncs;
	u32* nidList;
	u32* stubs;
} __attribute__((packed));


void resolveSyscalls(void)
{
	struct SceLibStubEntry* stubEntry = module_info.stub_top;
	int i;
	u32 *nid, *stub;

	u32	syscall, index;
	register u32 *temp_reg asm("$8");
	asm("cfc0  $t0, $12\n");

	u32 *syscall_table = (u32*)temp_reg[0];
	u32 syscall_offset = (u32)syscall_table[1];
	syscall_offset <<= 4;

	u32 *temp2 = (u32*)sceIoOpen;
	syscall = *(temp2+1);

	while (stubEntry < (struct SceLibStubEntry*)module_info.stub_end)
	{
		for (i = 0; i < stubEntry->numVars+stubEntry->numFuncs; i++)
		{
			stub = stubEntry->stubs;
			stub += i * 2; // since each stub is 8 bytes
			nid = stubEntry->nidList;
			nid += i;

			syscall = *(stub+1);
			
			if(syscall)
			{
				index = ((syscall - syscall_offset) >> 4);

				*stub = MIPS_J(syscall_table[4 + index/4]);
				*(stub+1) = 0;
			}
		}

		stubEntry += 1;
	}

	asmKernelDcacheWritebackInvalidateAll();
}

void PatchFlashDriver()
{
	//_sw(0, 0x88093ad0);
	// _sw(0, 0x880a35c0);  // this is for 2.71
	_sw(0, 0x080505a0);  // this is for 2.80
}


// This kernel proc just creates a new kmode thread, which is required to
// avoid the system being confused by the current thread, which is really a
// user thread interrupted during kernel ops.
void kernel_proc(void)
{	
	// We resolve all our syscalls to direct jumps, so that they will work from
	// kmode.
	resolveSyscalls();

	SceUID (*sceKernelCreateThread_k)(const char *name, SceKernelThreadEntry entry, int initPriority,
                             int stackSize, SceUInt attr, SceKernelThreadOptParam *option);
	int (*sceKernelStartThread_k)(SceUID thid, SceSize arglen, void *argp);

	// We find the createthread calls by hand, since these are actually
	// pointing to eloader code, rather than simple syscall stubs.
	sceKernelCreateThread_k = (void *)FindProc("sceThreadManager", "ThreadManForUser", 0x446d8de6);
	sceKernelStartThread_k = (void *)FindProc("sceThreadManager", "ThreadManForUser", 0xf475845d);

	pspSdkSetK1(0); // until we make our true kmode thread, we need to setk1
	                // before each syscall.
	
	int threadHandle = sceKernelCreateThread_k("kthrd", (void*)(0x80000000L | ((unsigned int)threadkproc)), 0x11, 0x40000, 0, 0);
	if (threadHandle < 0)
	{
		fillvram(0x000000FFL);
		printf("failed to create thread: %08X", threadHandle);
	}
	else
	{
		pspSdkSetK1(0);
		sceKernelStartThread_k(threadHandle, 0, 0);
	}

	pspSdkSetK1(0);
	sceKernelSleepThreadCB();   // might be able to get away with exitthread.
}

// -------------------------------------------
// Program entry point
int main(int argc, char* argv[])
{
	pspDebugScreenInit();

	sceRegOpenRegistry(0, 0, (void *) 0xbc000000);
	sceRegOpenRegistry(0, 0, (void *) 0xbc000004);
	sceRegOpenRegistry(0, 0, (void *) 0xbc000008);
	sceRegOpenRegistry(0, 0, (void *) 0xbc00000c);
	sceRegOpenRegistry(0, 0, (void *) 0xbc000010);
	sceRegOpenRegistry(0, 0, (void *) 0xbc000014);
	sceRegOpenRegistry(0, 0, (void *) 0xbc000018);
	sceRegOpenRegistry(0, 0, (void *) 0xbc00001c);

	u32 *LibcGettimeofday = (u32*)0x0800C294;	// 2.80
//	u32 *LibcGettimeofday = (u32*)0x0800be88;	// 2.71
//	u32 *LibcGettimeofday = (u32*)0x08057c74;	// 1.50
	*LibcGettimeofday = MIPS_J(kernel_proc);
	*(LibcGettimeofday+1) = 0;

	PatchFlashDriver();

	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheClearAll();

  fillvram(0);

	struct timeval tp;
	struct timezone tzp;

	pspDebugScreenPrintf("starting, wish me luck...\n");

	sceKernelDelayThread(500000);

	sceKernelLibcGettimeofday(&tp, &tzp);

	pspDebugScreenPrintf("hmm, why am I here?\n");

	return(0);
}

