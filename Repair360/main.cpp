#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DexFile.h"
#include "Leb128.h"


#define BYTE4_ALIGN(a) ( ((a) % 4) ? ((((a) >> 2) << 2) + 4): (a))
u1 opcode_table[256] = { 0 };
bool isLookUpTable = false;
bool decryptMode = false;

int Opcode_Len[256] = {
	1, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 2, 3, 2, 2, 3, 5, 2, 2, 3, 2, 1, 1, 2,
	2, 1, 2, 2, 3, 3, 3, 1, 1, 2, 3, 3, 3, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0,
	0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
	3, 3, 3, 0, 3, 3, 3, 3, 3, 0, 0, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 3, 3,
	3, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2, 0,

};


int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("Too few parameters\n");
		return -1;
	}
	if (strcmp(argv[1], "-g") == 0) {
		printf("You chose to generate the command map\n");
		if (argc != 5 && argc != 6)
		{
			printf("Not enough parameters, format: -g source.dex encrypt.dex file_path [opcode_table]\n");
			return -1;
		}
		decryptMode = false;
		generateMapFile(argc, argv);

	}
	else if (strcmp(argv[1], "-d") == 0) {
		printf("You have chosen to repair the file mode\n");
	}
	if (false)
	{
		//这里注意要必须指定为二进制格式读, 如果以只读方式读取文件可能会导致部分文件读取数据错误
		FILE *fp = fopen(argv[1], "rb");
		if (fp == NULL) {
			printf("open dex file error!\n");
			return -1;
		}
		fseek(fp, 0, SEEK_END);
		size_t dex_len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		printf("dex file length: 0x%x\n", dex_len);
		u1* data = (u1*)malloc(dex_len);
		if (data == NULL) {
			printf("malloc memory fail!\n");
			return -1;
		}
		fread(data, 1, dex_len, fp);
		fclose(fp);
		//读取opcode_table表, 配置表以256字节大小的二进制文件,是在libjiagu.so中的真实so中的struc_dexfile2结构体中提取出来的
		FILE *fp_opcode = fopen(argv[2], "rb");
		if (fp_opcode == NULL)
		{
			printf("open opcode_table file error!\n");
			return -1;
		}
		fseek(fp_opcode, 0, SEEK_END);
		size_t table_len = ftell(fp_opcode);
		fseek(fp_opcode, 0, SEEK_SET);
		if (table_len != 256)
		{
			printf("opcode_table file format error, Its size must be 256 bytes of binary\n");
			return -1;
		}

		fread(opcode_table, 256, 1, fp_opcode);
		fclose(fp_opcode);
		DexFile* pDexFile = dexFileParse(data, dex_len, 0);
		if (pDexFile == NULL) {
			printf("read dex file fail!\n");
			return -1;
		}
		findNativeMethod(pDexFile);
		free(data);
	}
	
	getchar();
	return 0;
}

int generateMapFile(int argc, char** argv)
{	
	//这里注意要必须指定为二进制格式读, 如果以只读方式读取文件可能会导致部分文件读取数据错误
	FILE *fp = fopen(argv[2], "rb");
	if (fp == NULL) {
		printf("open %s dex file error!\n", argv[2]);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	size_t source_dex_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	printf("source dex file length: 0x%x\n", source_dex_len);
	u1* source_data = (u1*)malloc(source_dex_len);
	if (source_data == NULL) {
		printf("malloc memory fail!\n");
		return -1;
	}
	fread(source_data, source_dex_len, 1, fp);
	fclose(fp);

	FILE *fp2 = fopen(argv[3], "rb");
	if (fp2 == NULL) {
		printf("open %s dex file error!\n", argv[3]);
		return -1;
	}
	fseek(fp2, 0, SEEK_END);
	size_t encrypt_dex_len = ftell(fp2);
	fseek(fp2, 0, SEEK_SET);
	printf("encrypt dex file length: 0x%x\n", encrypt_dex_len);
	u1* encrypt_data = (u1*)malloc(encrypt_dex_len);
	if (encrypt_data == NULL) {
		printf("malloc memory fail!\n");
		return -1;
	}
	fread(encrypt_data, encrypt_dex_len, 1, fp2);
	fclose(fp2);
	if (argc == 5)
	{
		printf("currently there is no mapping mode\n ");
		isLookUpTable = false;
	}
	else
	{
		printf("currently there is mapping mode\n ");
		isLookUpTable = true;
		FILE *fp3 = fopen(argv[5], "rb");
		if (fp3 == NULL)
		{
			printf("open %s opcode_table file error!\n", argv[5]);
			return -1;
		}
		fseek(fp3, 0, SEEK_END);
		size_t table_len = ftell(fp3);
		fseek(fp3, 0, SEEK_SET);
		if (table_len != 256)
		{
			printf("opcode_table file format error, Its size must be 256 bytes of binary\n");
			return -1;
		}

		fread(opcode_table, 256, 1, fp3);
		fclose(fp3);
	}

	FILE *out_fp = fopen(argv[4], "wt+");
	if (out_fp == NULL)
	{
		printf("open %s file failed\n", argv[4]);
		return -1;
	}
	char table[256][100] = { 0 };
	DexFile* pSourceDexFile = dexFileParse(source_data, source_dex_len, 0);
	DexFile* pEncryptDexFile = dexFileParse(encrypt_data, encrypt_dex_len, 0);
	findNativeMethodAndGenerateMap(table, pSourceDexFile, pEncryptDexFile, 0x80);
	//保存映射表到文件, 在table表中有些行是空行,如没有使用的opcode和dex文件中不存在的和没有包含的指令(throw, return, return-wide, return-object),通常不影响,加固后也使用不到这些指令
	for (int i = 0; i < 256; i++)
	{
		if (*table[i] =='\0')
		{
			strcpy(table[i], "0x");
			if (i < 0x10)
			{
				strcat(table[i], "0");
			}
			char string[7];
			itoa(i, string, 16);
			strcat(table[i], string);
			strcat(table[i], " ");			
			strcat(table[i], "NULL");
			strcat(table[i], " ");
			itoa(Opcode_Len[i], string, 10);
			strcat(table[i], string);
			strcat(table[i], " ");
			strcat(table[i], dexGetOpcodeName((Opcode)i));
		}
		fputs(table[i], out_fp);
		fputc('\n', out_fp);
	}
	fclose(out_fp);
	free(source_data);
	free(encrypt_data);
	return 0;
}

void generateMapTable(char table[256][100],const DexCode* sourceData, const DexCode* encryptData, u1 key, bool lookUpTable) 
{
	u4 insnsSize = sourceData->insnsSize;
	//默认情况下伪指令是在指令的最后部分, 因此指令查找到伪指令起始指令即可,但是要注意伪指令必须位于偶数字节码偏移(4字节对齐), 因此之前一个指令可能为nop指令
	u4 insns_end_offset = insnsSize;
	u4 offset = 0;
	bool hasDirective = false;
	for (u4 i = 0; i < insns_end_offset; i++)
	{
		Opcode opcode = dexOpcodeFromCodeUnit(sourceData->insns[i]);
		Opcode encrypt_opcode;
		if (lookUpTable) {
			encrypt_opcode = (Opcode)opcode_table[dexOpcodeFromCodeUnit(encryptData->insns[i]) ^ key];
		}
		else
		{
			encrypt_opcode = (Opcode)(dexOpcodeFromCodeUnit(encryptData->insns[i]) ^ key);
		}
		// 这里要考虑packed-switch-payload, sparse-switch-payload, fill-array-data-payload三种伪指令的情况
		printf("当前指令: %s\n", dexGetOpcodeName(opcode));
		//packed-switch vAA, +BBBBBBBB  fill-array-data vAA, +BBBBBBBB  sparse-switch vAA, +BBBBBBBB
		if (opcode == OP_PACKED_SWITCH || opcode == OP_SPARSE_SWITCH || opcode == OP_FILL_ARRAY_DATA) {			
			printf("%s指令索引: %d  ", dexGetOpcodeName(opcode), i);
			offset = (sourceData->insns[i + 1] | (sourceData->insns[i + 2] << 16)) + i;
			printf("%s 数据在指令中索引: %d\n", dexGetOpcodeName(opcode), offset);
			if (offset < insns_end_offset)
			{
				insns_end_offset = offset;
				printf("更新伪指令起始指令索引: %d\n", insns_end_offset);
			}
			hasDirective = true;
		}
		if (hasDirective && i == (insns_end_offset - 1))	//由于伪指令4字节对齐的原因可能这条指令为空指令
		{
			if (opcode == OP_NOP)
			{
				break;
			}
		}
		int len = Opcode_Len[opcode];
		char string[7];

		strcpy(table[opcode], "0x");
		if (opcode < 0x10)
		{
			strcat(table[opcode], "0");
		}
		itoa(opcode, string, 16);
		strcat(table[opcode], string);
		strcat(table[opcode], " ");
		strcat(table[opcode], "0x");
		if (encrypt_opcode < 0x10)
		{
			strcat(table[opcode], "0");
		}
		itoa(encrypt_opcode, string, 16);
		strcat(table[opcode], string);
		strcat(table[opcode], " ");
		itoa(len, string, 10);
		strcat(table[opcode], string);
		strcat(table[opcode], " ");
		strcat(table[opcode], dexGetOpcodeName(opcode));
		i = i + len - 1;
	}
	
}

void decryptDexCode(u1** ptr_insns, u4 len, u4 registersSize, u4 classIdx, u4 methodIdx, u4 key, bool isLookUpTable)
{
	u1 xor_key = (registersSize ^ classIdx ^ methodIdx ^ key) & 0xff;
}

void findNativeMethodAndGenerateMap(char table[256][100], const DexFile* pSourceDexFile, const DexFile* pEncryptDexFile, u1 key_360)
{
	const DexClassDef* pEnDexClassDef = (const DexClassDef*)malloc(sizeof(DexClassDef));
	const DexCode* pEnDexCode = (const DexCode*)malloc(sizeof(DexCode));
	ClassData* pEnClassData = (ClassData*)malloc(sizeof(ClassData));
	const DexMethodId* pEnDexMethodId = (const DexMethodId*)malloc(sizeof(DexMethodId));
	u4 lastEnDexCodeOff = 0;
	u1 key = 0;
	for (u4 i = 0; i < pEncryptDexFile->pHeader->classDefsSize; i++)
	{
		pEnDexClassDef = dexGetClassDef(pEncryptDexFile, i);
		if (pEnDexClassDef->classDataOff != 0)
		{
			pEnClassData = dexGetClassData(pEncryptDexFile, pEnClassData, pEncryptDexFile->baseAddr + pEnDexClassDef->classDataOff);
			int baseMethodIndex = 0;
			for (int j = 0; j < pEnClassData->directMethodsSize + pEnClassData->virtualMethodsSize; j++)
			{
				if (j == 0) {
					baseMethodIndex = pEnClassData->encodedMethod[0].methodIdxDiff;
				}
				else if (j == pEnClassData->directMethodsSize) {
					baseMethodIndex = pEnClassData->encodedMethod[j].methodIdxDiff;
				}
				else {
					baseMethodIndex += pEnClassData->encodedMethod[j].methodIdxDiff;
				}

				if (pEnClassData->encodedMethod[j].codeOff == NULL && (pEnClassData->encodedMethod[j].accessFlags & ACC_NATIVE))
				{
					pEnDexMethodId = dexGetMethodId(pEncryptDexFile, baseMethodIndex);
					const char* methodName = dexStringById(pEncryptDexFile, pEnDexMethodId->nameIdx);
					if (strcmp(methodName, "onCreate") == 0)
					{
						pEnDexCode = (const DexCode *)(pEncryptDexFile->baseAddr + lastEnDexCodeOff);
						key = (pEnDexMethodId->classIdx ^ baseMethodIndex ^ pEnDexCode->registersSize ^ key_360) & 0xff;
						// 这里加固后ClassDef索引不会变,但是methodIdx是可能变化的,因为代码中增加了静态方法<clinit>, 可能导致方法数增加,因此需要重新查找源文件onCreate方法的索引
						DexCode* pSDexCode = getSourceDexCode(pSourceDexFile, NULL, i);
						generateMapTable(table, pSDexCode, pEnDexCode, key, isLookUpTable);
					}
				}
				else if (pEnClassData->encodedMethod[j].codeOff != NULL)
				{
					if (lastEnDexCodeOff == 0) {
						lastEnDexCodeOff = (const u1 *)pEnClassData->encodedMethod[j].codeOff - pEncryptDexFile->baseAddr;
					}
					pEnDexCode = pEnClassData->encodedMethod[j].codeOff;
					// 读取DexCode的insns,找到insns的结束位置
					lastEnDexCodeOff = BYTE4_ALIGN((const u1*)&pEnDexCode->insns[pEnDexCode->insnsSize] - pEncryptDexFile->baseAddr);
				}
			}
		}
	}
}

DexCode* getSourceDexCode(const DexFile* pSDexFile, DexCode* pSDexCode, u4 classDefIdx)
{
	if (pSDexCode == NULL)
	{
		pSDexCode = (DexCode* )malloc(sizeof(DexCode));
	}
	const DexClassDef* pSDexClassDef = (const DexClassDef*)malloc(sizeof(DexClassDef));
	pSDexClassDef = dexGetClassDef(pSDexFile, classDefIdx);
	ClassData* pSClassData = (ClassData*)malloc(sizeof(ClassData));
	pSClassData = dexGetClassData(pSDexFile, pSClassData, pSDexFile->baseAddr + pSDexClassDef->classDataOff);
	u4 baseMethodIdx = 0;
	const DexMethodId* pDexMethodId = (const DexMethodId*)malloc(sizeof(DexMethodId));
	for (int i = 0; i < pSClassData->directMethodsSize + pSClassData->virtualMethodsSize; i++)
	{
		if (i == 0)
		{
			baseMethodIdx = pSClassData->encodedMethod[i].methodIdxDiff;
		}
		else if (i == pSClassData->directMethodsSize) {
			baseMethodIdx = pSClassData->encodedMethod[i].methodIdxDiff;
		}
		else
		{
			baseMethodIdx += pSClassData->encodedMethod[i].methodIdxDiff;
		}
		pDexMethodId = dexGetMethodId(pSDexFile, baseMethodIdx);
		const char* method_name = dexStringById(pSDexFile, pDexMethodId->nameIdx);
		if (strcmp(method_name, "onCreate") == 0)
		{
			pSDexCode = pSClassData->encodedMethod[i].codeOff;
			break;
		}
	}
	return pSDexCode;
}

void findNativeMethod(const DexFile* pDexFile)
{
	const DexClassDef* pDexClassDef = (const DexClassDef*)malloc(sizeof(DexClassDef));
	const DexCode* pDexCode = (const DexCode*)malloc(sizeof(DexCode));
	ClassData* pClassData = (ClassData*)malloc(sizeof(ClassData));
	const DexMethodId* pDexMethodId = (const DexMethodId*)malloc(sizeof(DexMethodId));
	u4 lastDexCodeOff = 0;
	int methodExtractNumber = 0;
	for (u4 i = 0; i < pDexFile->pHeader->classDefsSize; i++) {
		pDexClassDef = dexGetClassDef(pDexFile, i);
		if (pDexClassDef->classDataOff != 0)
		{
			pClassData = dexGetClassData(pDexFile, pClassData, pDexFile->baseAddr + pDexClassDef->classDataOff);
			int baseMethodIndex = 0;
			for (int j = 0; j < pClassData->directMethodsSize + pClassData->virtualMethodsSize; j++) {

				if (j == 0) {
					baseMethodIndex = pClassData->encodedMethod[0].methodIdxDiff;
				}
				else if (j == pClassData->directMethodsSize) {
					baseMethodIndex = pClassData->encodedMethod[j].methodIdxDiff;
				}
				else {
					baseMethodIndex += pClassData->encodedMethod[j].methodIdxDiff;
				}

				// abstract method and native method DexCode = 0
				if (pClassData->encodedMethod[j].codeOff == NULL && (pClassData->encodedMethod[j].accessFlags & ACC_NATIVE)) {
					pDexMethodId = dexGetMethodId(pDexFile, baseMethodIndex);
					const char* methodName = dexStringById(pDexFile, pDexMethodId->nameIdx);
					if (strcmp(methodName, "onCreate") == 0) {
						printf("ClassDef Index : %d, native method Index: %d\n", i, baseMethodIndex);
						printMethodStringById(pDexFile, baseMethodIndex, pClassData->encodedMethod[j].accessFlags);
						pDexCode = (const DexCode *)(pDexFile->baseAddr + lastDexCodeOff);
						printDexCodeStructure(pDexFile, pDexCode);
						methodExtractNumber++;
						lastDexCodeOff = 0;				//这里由于360加密后指令还在原来的位置,但后面又加了一些可能和类有关系的指令,因此需要重新修正偏移,且由于360只加工onCreate方法,所以不用考虑连续的两个被360加固的方法
					}
				}
				else if (pClassData->encodedMethod[j].codeOff != NULL)
				{
					if (lastDexCodeOff == 0) {
						lastDexCodeOff = (const u1 *)pClassData->encodedMethod[j].codeOff - pDexFile->baseAddr;
					}
					pDexCode = pClassData->encodedMethod[j].codeOff;
					// 读取DexCode的insns,找到insns的结束位置
					lastDexCodeOff = BYTE4_ALIGN((const u1*)&pDexCode->insns[pDexCode->insnsSize] - pDexFile->baseAddr);
				}
			}
		}
		else {			// handle ClassData is null.

		}
	}
	//通过查找被加密的onCreate数量可知,在Service和Receiver里面也调用了StubApp.interface11(XX),但目前方法还并未被加密,因此可以猜测后续版本可能会把4大组件的启动方法(onCreate,onStart等)都给加密掉
	printf("total was 360 encrypt number: %d\n", methodExtractNumber);
}

void printDexCodeStructure(const DexFile* pDexFile, const DexCode* pDexCode) {
	printf("current DexCode in memory address: 0x%p, DexCode in dex file offset: 0x%x\n", pDexCode, (u1*)pDexCode - pDexFile->baseAddr);
	printf("register number: %d, ins number: %d, outs number: %d, try catch number: %d, debug offset: 0x%x", pDexCode->registersSize, pDexCode->insSize, pDexCode->outsSize, pDexCode->triesSize, pDexCode->debugInfoOff);
	printf("insns number: 0x%x  insns: \n", pDexCode->insnsSize);

	for (u4 i = 0; i < pDexCode->insnsSize; i++)
	{
		printf("0x%x ", pDexCode->insns[i]);
	}
	printf("\n");
}

ClassData* dexGetClassData(const DexFile* pDexFile, ClassData* pClassData, const u1* data) {
	if (pClassData == NULL) {
		pClassData = (ClassData*)malloc(sizeof(ClassData));
	}
	pClassData->staticFieldsSize = readUnsignedLeb128(&data);
	pClassData->instanceFieldsSize = readUnsignedLeb128(&data);
	pClassData->directMethodsSize = readUnsignedLeb128(&data);
	pClassData->virtualMethodsSize = readUnsignedLeb128(&data);
	if ((pClassData->staticFieldsSize + pClassData->instanceFieldsSize) > 0) {
		pClassData->encodedField = (ClassDataOfField*)malloc(sizeof(ClassDataOfField) * (pClassData->staticFieldsSize + pClassData->instanceFieldsSize));
		for (int i = 0; i < pClassData->staticFieldsSize + pClassData->instanceFieldsSize; i++) {
			pClassData->encodedField[i].fieldIdxDiff = readUnsignedLeb128(&data);
			pClassData->encodedField[i].accessFlags = readUnsignedLeb128(&data);
		}
	}
	if ((pClassData->directMethodsSize + pClassData->virtualMethodsSize) > 0) {
		pClassData->encodedMethod = (ClassDataOfMethod*)malloc(sizeof(ClassDataOfMethod) * (pClassData->directMethodsSize + pClassData->virtualMethodsSize));
		for (int i = 0; i < pClassData->directMethodsSize + pClassData->virtualMethodsSize; i++) {
			pClassData->encodedMethod[i].methodIdxDiff = readUnsignedLeb128(&data);
			pClassData->encodedMethod[i].accessFlags = readUnsignedLeb128(&data);
			u4 offset = readUnsignedLeb128(&data);
			if (offset != 0) {
				pClassData->encodedMethod[i].codeOff = (DexCode *)(pDexFile->baseAddr + offset);
			}
			else {
				pClassData->encodedMethod[i].codeOff = NULL;
			}
		}
	}
	return pClassData;
}

void printMethodStringById(const DexFile* pDexFile, u4 idx, int methodAccessFlags)
{
	const DexMethodId* pDexMethodId = (const DexMethodId*)malloc(sizeof(DexMethodId));
	pDexMethodId = dexGetMethodId(pDexFile, idx);
	const char* methodName = dexStringById(pDexFile, pDexMethodId->nameIdx);


	const char* className = dexStringByTypeIdx(pDexFile, pDexMethodId->classIdx);


	const DexProtoId* pDexProtoId = (const DexProtoId*)malloc(sizeof(DexProtoId));
	pDexProtoId = dexGetProtoId(pDexFile, pDexMethodId->protoIdx);
	const char* returnTypeName = dexStringByTypeIdx(pDexFile, pDexProtoId->returnTypeIdx);


	int parametersNum = 0;

	const DexTypeList* pDexTypeList = (const DexTypeList *)malloc(sizeof(DexTypeList));
	pDexTypeList = dexGetProtoParameters(pDexFile, pDexProtoId);
	const char** parametersName = NULL;
	if (pDexTypeList != NULL) {
		if (pDexTypeList->size) {
			parametersName = (const char**)malloc(sizeof(1) * pDexTypeList->size);

			parametersNum = pDexTypeList->size;
			for (u4 i = 0; i < pDexTypeList->size; i++) {
				DexTypeItem pDexTypeItem = pDexTypeList->list[i];
				parametersName[i] = dexStringByTypeIdx(pDexFile, pDexTypeItem.typeIdx);
			}
		}
	}

	if (strlen(returnTypeName) == 1) {
		returnTypeName = dexGetLongTypeDescriptor(returnTypeName[0]);
	}
	else {
		returnTypeName = type2LongString(returnTypeName);
	}
	className = type2LongString(className);
	for (int i = 0; i < parametersNum; i++) {
		if (strlen(parametersName[i]) == 1) {
			parametersName[i] = dexGetLongTypeDescriptor(parametersName[i][0]);
		}
		else {
			parametersName[i] = type2LongString(parametersName[i]);
		}
	}
	char access[100] = { 0 };
	accessFlags2String(access, methodAccessFlags);
	printf("%s%s %s.%s(", access, returnTypeName, className, methodName);
	for (int i = 0; i < parametersNum; i++) {
		if (i != parametersNum - 1) {
			printf("%s, ", parametersName[i]);
		}
		else {
			printf("%s", parametersName[i]);
		}

	}
	printf(")\n");

}

const char* accessFlags2String(char* str, int access)
{
	if (access & ACC_PUBLIC) {
		strcat(str, "public ");
	}
	else if (access & ACC_PRIVATE) {
		strcat(str, "private ");
	}
	else if (access & ACC_PROTECTED) {
		strcat(str, "protect ");
	}
	if (access & ACC_STATIC)
		strcat(str, "static ");
	if (access & ACC_FINAL)
		strcat(str, "final ");
	if (access & ACC_SYNCHRONIZED)
		strcat(str, "synchronized ");
	if (access & ACC_BRIDGE)
		strcat(str, "bridge ");
	if (access & ACC_VARARGS)
		strcat(str, "varages ");
	if (access & ACC_NATIVE)
		strcat(str, "native ");
	if (access & ACC_ABSTRACT)
		strcat(str, "abstract ");
	if (access & ACC_STRICT)
		strcat(str, "strict ");
	return (const char*)str;
}

/*Lcom/beichen/test;  --->  com.beichen.test*/
const char* type2LongString(const char* name)
{
	int len = strlen(name);
	char *dest = (char *)malloc(len - 1);
	//拷贝是去掉开头 "L" 和结尾 ";"
	memcpy(dest, name + 1, len - 2);
	dest[len - 2] = 0;
	for (int i = 0; i < len - 1; i++) {
		if (dest[i] == '/')
			dest[i] = '.';
	}
	return (const char*)dest;
}

void printDexClassDataStructure(const DexFile* pDexFile, const ClassData* pClassData)
{
	printf("staticFieldsSize: %d, instanceFieldsSize: %d, directMethodsSize: %d, virtualMethodsSize: %d\n", pClassData->staticFieldsSize, pClassData->instanceFieldsSize, pClassData->directMethodsSize, pClassData->virtualMethodsSize);
	printf("encodedField pointer: 0x%p, encodedMethod pointer: 0x%p\n", pClassData->encodedField, pClassData->encodedMethod);
	printf("print Field:\n");
	for (int i = 0; i < pClassData->staticFieldsSize + pClassData->instanceFieldsSize; i++) {
		printf("Field index : %d  FieldIdxDiff : %d  FieldAccessFlags : 0x%x\n", i, pClassData->encodedField[i].fieldIdxDiff, pClassData->encodedField[i].accessFlags);
	}
	printf("print Method:\n");
	for (int i = 0; i < pClassData->directMethodsSize + pClassData->virtualMethodsSize; i++) {
		printf("Method index : %d MethodIdxOff : 0x%x  MethodAccessFlags : 0x%x\n  DexCode pointer : 0x%x\n", i, pClassData->encodedMethod[i].methodIdxDiff, pClassData->encodedMethod[i].accessFlags, (const u1*)pClassData->encodedMethod[i].codeOff - pDexFile->baseAddr);
	}
}

void printDexCodeStructure(const DexCode* pDexCode) {
	printf("registersSize: %d, insSize: %d, outsSize: %d, triesSize: %d\n", pDexCode->registersSize, pDexCode->insSize, pDexCode->outsSize, pDexCode->triesSize);
	printf("debufInfoOff: 0x%x, insnsSize: 0x%x, insns[0]: 0x%x\n", pDexCode->debugInfoOff, pDexCode->insnsSize, pDexCode->insns[0]);
}
void printDexClassDefStructure(const DexClassDef* pDexClassDef) {
	printf("classIdx: 0x%x, accessFlags: 0x%x, superclassIdx: 0x%x, interfacesOff: 0x%x\n", pDexClassDef->classIdx, pDexClassDef->accessFlags, pDexClassDef->superclassIdx, pDexClassDef->interfacesOff);
	printf("sourceFileIdx: 0x%x, annotationsOff: 0x%x, classDataOff: 0x%x, staticValuesOff: 0x%x\n", pDexClassDef->sourceFileIdx, pDexClassDef->annotationsOff, pDexClassDef->classDataOff, pDexClassDef->staticValuesOff);
}
void printDexHeadStructure(const DexFile* pDexFile)
{
	printf("stringIdsSize: %d, stringIdsOff: 0x%x\n", pDexFile->pHeader->stringIdsSize, pDexFile->pHeader->stringIdsOff);
	printf("typeIdsSize: %d, typeIdsOff: 0x%x\n", pDexFile->pHeader->typeIdsSize, pDexFile->pHeader->typeIdsOff);
	printf("protoIdsSize: %d, protoIdsOff: 0x%x\n", pDexFile->pHeader->protoIdsSize, pDexFile->pHeader->protoIdsOff);
	printf("fieldIdsSize: %d, fieldIdsOff: 0x%x\n", pDexFile->pHeader->fieldIdsSize, pDexFile->pHeader->fieldIdsOff);
	printf("methodIdsSize: %d, methodIdsOff: 0x%x\n", pDexFile->pHeader->methodIdsSize, pDexFile->pHeader->methodIdsOff);
	printf("classDefsSize: %d, classDefsOff: 0x%x\n", pDexFile->pHeader->classDefsSize, pDexFile->pHeader->classDefsOff);
	printf("dataSize: %d, dataOff: 0x%x\n", pDexFile->pHeader->dataSize, pDexFile->pHeader->dataOff);
}

DexFile* dexFileParse(const u1* data, size_t length, int flags)
{
	DexFile* pDexFile = NULL;
	const DexHeader* pHeader;
	int result = -1;
	if (length < sizeof(DexHeader)) {
		printf("too short to be a valid .dex");
		goto bail;
	}
	pDexFile = (DexFile*)malloc(sizeof(DexFile));
	if (pDexFile == NULL)
		goto bail;
	memset(pDexFile, 0, sizeof(DexFile));

	dexFileSetupBasicPointers(pDexFile, data);
	pHeader = pDexFile->pHeader;

	result = 0;
bail:
	if (result != 0 && pDexFile != NULL) {
		dexFileFree(pDexFile);
		pDexFile = NULL;
	}
	return pDexFile;
}
void dexFileFree(DexFile* pDexFile)
{
	if (pDexFile == NULL)
		return;

	free(pDexFile);
}

void dexFileSetupBasicPointers(DexFile* pDexFile, const u1* data) {
	DexHeader *pHeader = (DexHeader*)data;

	pDexFile->baseAddr = data;
	pDexFile->pHeader = pHeader;
	pDexFile->pStringIds = (const DexStringId*)(data + pHeader->stringIdsOff);
	pDexFile->pTypeIds = (const DexTypeId*)(data + pHeader->typeIdsOff);
	pDexFile->pFieldIds = (const DexFieldId*)(data + pHeader->fieldIdsOff);
	pDexFile->pMethodIds = (const DexMethodId*)(data + pHeader->methodIdsOff);
	pDexFile->pProtoIds = (const DexProtoId*)(data + pHeader->protoIdsOff);
	pDexFile->pClassDefs = (const DexClassDef*)(data + pHeader->classDefsOff);
	pDexFile->pLinkData = (const DexLink*)(data + pHeader->linkOff);
}

/* (documented in header) */
const char* dexGetPrimitiveTypeDescriptor(PrimitiveType type) {
	switch (type) {
	case PRIM_VOID:    return "V";
	case PRIM_BOOLEAN: return "Z";
	case PRIM_BYTE:    return "B";
	case PRIM_SHORT:   return "S";
	case PRIM_CHAR:    return "C";
	case PRIM_INT:     return "I";
	case PRIM_LONG:    return "J";
	case PRIM_FLOAT:   return "F";
	case PRIM_DOUBLE:  return "D";
	default:           return NULL;
	}

	return NULL;
}

const char* dexGetLongTypeDescriptor(const char type) {
	switch (type) {
	case 'V':	return "void";
	case 'Z':	return "boolean";
	case 'B':	return "byte";
	case 'S':	return "short";
	case 'C':	return "char";
	case 'I':	return "int";
	case 'J':	return "long";
	case 'F':	return "float";
	case 'D':	return "double";
	default:	return NULL;
	}
	return NULL;
}

/* (documented in header) */
const char* dexGetBoxedTypeDescriptor(PrimitiveType type) {
	switch (type) {
	case PRIM_VOID:    return NULL;
	case PRIM_BOOLEAN: return "Ljava/lang/Boolean;";
	case PRIM_BYTE:    return "Ljava/lang/Byte;";
	case PRIM_SHORT:   return "Ljava/lang/Short;";
	case PRIM_CHAR:    return "Ljava/lang/Character;";
	case PRIM_INT:     return "Ljava/lang/Integer;";
	case PRIM_LONG:    return "Ljava/lang/Long;";
	case PRIM_FLOAT:   return "Ljava/lang/Float;";
	case PRIM_DOUBLE:  return "Ljava/lang/Double;";
	default:           return NULL;
	}
}

/* (documented in header) */
PrimitiveType dexGetPrimitiveTypeFromDescriptorChar(char descriptorChar) {
	switch (descriptorChar) {
	case 'V': return PRIM_VOID;
	case 'Z': return PRIM_BOOLEAN;
	case 'B': return PRIM_BYTE;
	case 'S': return PRIM_SHORT;
	case 'C': return PRIM_CHAR;
	case 'I': return PRIM_INT;
	case 'J': return PRIM_LONG;
	case 'F': return PRIM_FLOAT;
	case 'D': return PRIM_DOUBLE;
	default:  return PRIM_NOT;
	}
}


static const char* gOpNames[kNumPackedOpcodes] = {
	// BEGIN(libdex-opcode-names); GENERATED AUTOMATICALLY BY opcode-gen
	"nop",
	"move",
	"move/from16",
	"move/16",
	"move-wide",
	"move-wide/from16",
	"move-wide/16",
	"move-object",
	"move-object/from16",
	"move-object/16",
	"move-result",
	"move-result-wide",
	"move-result-object",
	"move-exception",
	"return-void",
	"return",
	"return-wide",
	"return-object",
	"const/4",
	"const/16",
	"const",
	"const/high16",
	"const-wide/16",
	"const-wide/32",
	"const-wide",
	"const-wide/high16",
	"const-string",
	"const-string/jumbo",
	"const-class",
	"monitor-enter",
	"monitor-exit",
	"check-cast",
	"instance-of",
	"array-length",
	"new-instance",
	"new-array",
	"filled-new-array",
	"filled-new-array/range",
	"fill-array-data",
	"throw",
	"goto",
	"goto/16",
	"goto/32",
	"packed-switch",
	"sparse-switch",
	"cmpl-float",
	"cmpg-float",
	"cmpl-double",
	"cmpg-double",
	"cmp-long",
	"if-eq",
	"if-ne",
	"if-lt",
	"if-ge",
	"if-gt",
	"if-le",
	"if-eqz",
	"if-nez",
	"if-ltz",
	"if-gez",
	"if-gtz",
	"if-lez",
	"unused-3e",
	"unused-3f",
	"unused-40",
	"unused-41",
	"unused-42",
	"unused-43",
	"aget",
	"aget-wide",
	"aget-object",
	"aget-boolean",
	"aget-byte",
	"aget-char",
	"aget-short",
	"aput",
	"aput-wide",
	"aput-object",
	"aput-boolean",
	"aput-byte",
	"aput-char",
	"aput-short",
	"iget",
	"iget-wide",
	"iget-object",
	"iget-boolean",
	"iget-byte",
	"iget-char",
	"iget-short",
	"iput",
	"iput-wide",
	"iput-object",
	"iput-boolean",
	"iput-byte",
	"iput-char",
	"iput-short",
	"sget",
	"sget-wide",
	"sget-object",
	"sget-boolean",
	"sget-byte",
	"sget-char",
	"sget-short",
	"sput",
	"sput-wide",
	"sput-object",
	"sput-boolean",
	"sput-byte",
	"sput-char",
	"sput-short",
	"invoke-virtual",
	"invoke-super",
	"invoke-direct",
	"invoke-static",
	"invoke-interface",
	"unused-73",
	"invoke-virtual/range",
	"invoke-super/range",
	"invoke-direct/range",
	"invoke-static/range",
	"invoke-interface/range",
	"unused-79",
	"unused-7a",
	"neg-int",
	"not-int",
	"neg-long",
	"not-long",
	"neg-float",
	"neg-double",
	"int-to-long",
	"int-to-float",
	"int-to-double",
	"long-to-int",
	"long-to-float",
	"long-to-double",
	"float-to-int",
	"float-to-long",
	"float-to-double",
	"double-to-int",
	"double-to-long",
	"double-to-float",
	"int-to-byte",
	"int-to-char",
	"int-to-short",
	"add-int",
	"sub-int",
	"mul-int",
	"div-int",
	"rem-int",
	"and-int",
	"or-int",
	"xor-int",
	"shl-int",
	"shr-int",
	"ushr-int",
	"add-long",
	"sub-long",
	"mul-long",
	"div-long",
	"rem-long",
	"and-long",
	"or-long",
	"xor-long",
	"shl-long",
	"shr-long",
	"ushr-long",
	"add-float",
	"sub-float",
	"mul-float",
	"div-float",
	"rem-float",
	"add-double",
	"sub-double",
	"mul-double",
	"div-double",
	"rem-double",
	"add-int/2addr",
	"sub-int/2addr",
	"mul-int/2addr",
	"div-int/2addr",
	"rem-int/2addr",
	"and-int/2addr",
	"or-int/2addr",
	"xor-int/2addr",
	"shl-int/2addr",
	"shr-int/2addr",
	"ushr-int/2addr",
	"add-long/2addr",
	"sub-long/2addr",
	"mul-long/2addr",
	"div-long/2addr",
	"rem-long/2addr",
	"and-long/2addr",
	"or-long/2addr",
	"xor-long/2addr",
	"shl-long/2addr",
	"shr-long/2addr",
	"ushr-long/2addr",
	"add-float/2addr",
	"sub-float/2addr",
	"mul-float/2addr",
	"div-float/2addr",
	"rem-float/2addr",
	"add-double/2addr",
	"sub-double/2addr",
	"mul-double/2addr",
	"div-double/2addr",
	"rem-double/2addr",
	"add-int/lit16",
	"rsub-int",
	"mul-int/lit16",
	"div-int/lit16",
	"rem-int/lit16",
	"and-int/lit16",
	"or-int/lit16",
	"xor-int/lit16",
	"add-int/lit8",
	"rsub-int/lit8",
	"mul-int/lit8",
	"div-int/lit8",
	"rem-int/lit8",
	"and-int/lit8",
	"or-int/lit8",
	"xor-int/lit8",
	"shl-int/lit8",
	"shr-int/lit8",
	"ushr-int/lit8",
	"+iget-volatile",
	"+iput-volatile",
	"+sget-volatile",
	"+sput-volatile",
	"+iget-object-volatile",
	"+iget-wide-volatile",
	"+iput-wide-volatile",
	"+sget-wide-volatile",
	"+sput-wide-volatile",
	"^breakpoint",
	"^throw-verification-error",
	"+execute-inline",
	"+execute-inline/range",
	"+invoke-object-init/range",
	"+return-void-barrier",
	"+iget-quick",
	"+iget-wide-quick",
	"+iget-object-quick",
	"+iput-quick",
	"+iput-wide-quick",
	"+iput-object-quick",
	"+invoke-virtual-quick",
	"+invoke-virtual-quick/range",
	"+invoke-super-quick",
	"+invoke-super-quick/range",
	"+iput-object-volatile",
	"+sget-object-volatile",
	"+sput-object-volatile",
	"unused-ff",
	// END(libdex-opcode-names)
};

/*
* Return the name of an opcode.
*/
const char* dexGetOpcodeName(Opcode op)
{
	assert(op >= 0 && op < kNumPackedOpcodes);
	return gOpNames[op];
}
