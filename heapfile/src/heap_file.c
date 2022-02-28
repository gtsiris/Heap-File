#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "heap_file.h"


#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

HP_ErrorCode HP_Init() {  /* The implementation does not require any extra structure, therefore there are not any initializations */
  return HP_OK;  /* Success */
}

HP_ErrorCode HP_CreateFile(const char *filename) {
  CALL_BF(BF_CreateFile(filename));  /* Create a file using BF level */
  int file_desc;  /* The identifying number of a file opening */
  CALL_BF(BF_OpenFile(filename, &file_desc));  /* Open the file and set file_desc to a valid value using BF level */
  BF_Block *first_block;  /* Block pointer for memory allocation and management */
  BF_Block_Init(&first_block);  /* Allocate and initialize memory for a block */
  CALL_BF(BF_AllocateBlock(file_desc, first_block));  /* Allocate memory for this block inside the open file */
  char *data = BF_Block_GetData(first_block);  /* Find where the block's data starts */
  memset(data, 'h', sizeof(char));  /* Write 'h', 'h' stands for heap file */
  BF_Block_SetDirty(first_block);  /* Changed data so mark block as dirty */
  CALL_BF(BF_UnpinBlock(first_block));  /* Unpin block */
  BF_Block_Destroy(&first_block);  /* Free the allocated memory */
  CALL_BF(BF_CloseFile(file_desc));  /* Close the file using BF level */
  return HP_OK;  /* Success */
}

HP_ErrorCode HP_OpenFile(const char *fileName, int *fileDesc) {
  CALL_BF(BF_OpenFile(fileName, fileDesc));  /* Open the file and set fileDesc to a valid value using BF level */
  BF_Block *first_block;  /* Block pointer for memory allocation and management */
  BF_Block_Init(&first_block);  /* Allocate and initialize memory for a block */
  CALL_BF(BF_GetBlock(*fileDesc, 0, first_block));  /* Get the first block of the open file using BF level (zero indexing) */
  char *data = BF_Block_GetData(first_block);  /* Find where the first block's data starts */
  if (*data != 'h') {  /* Check if it is a heap file */
  	CALL_BF(BF_UnpinBlock(first_block));  /* Unpin block */
    BF_Block_Destroy(&first_block);  /* Free the allocated memory */
  	return HP_ERROR;  /* It is not a heap file */
  }
  /* Data did not change, so block is not dirty */
  CALL_BF(BF_UnpinBlock(first_block));  /* Unpin block */
  BF_Block_Destroy(&first_block);  /* Free the allocated memory */
  return HP_OK;  /* Success */
}

HP_ErrorCode HP_CloseFile(int fileDesc) {
  CALL_BF(BF_CloseFile(fileDesc));  /* Close the file using BF level */
  return HP_OK;  /* Success */
}

HP_ErrorCode HP_InsertEntry(int fileDesc, Record record) {
  int blocks_num;  /* Number of blocks in the open file */
  CALL_BF(BF_GetBlockCounter(fileDesc, &blocks_num));  /* Get that number using BF level */
  BF_Block *block;  /* Block pointer for memory allocation and management */
  BF_Block_Init(&block);  /* Allocate and initialize memory for a block */
  Record *data;  /* Record pointer for memory management and allocation */
  if (blocks_num == 1) {  /* If there was only one block (records are not stored at first block) */
    CALL_BF(BF_AllocateBlock(fileDesc, block));  /* Allocate memory for another block inside the open file */
    int *record_counter = (int *)BF_Block_GetData(block);  /* First data of the block is the record_counter */
    data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
    *record_counter = 1;  /* The first record is going to be added, so set the record_counter to 1 */
  }
  else {  /* Else if there were more than blocks */
    CALL_BF(BF_GetBlock(fileDesc, blocks_num - 1, block));  /* Get the last block of the open file using BF level */
    int *record_counter = (int *)BF_Block_GetData(block);  /* First data of this block is the record_counter */
    if (((*record_counter) + 1) * sizeof(Record)  <= BF_BLOCK_SIZE - sizeof(int)) {  /* If there is enough memory in this block for another record */
	  data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
      data += *record_counter;  /* Go to the empty space of this block */
      (*record_counter)++;  /* A record is going to be added, so increase the record_counter by 1 */
    }
    else {  /* Else if space in the already existing blocks is not enough */
      CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
	  CALL_BF(BF_AllocateBlock(fileDesc, block));  /* Allocate memory for another block inside the open file */
      int *record_counter = (int *)BF_Block_GetData(block);  /* First data of this block is the record_counter */
      data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
      *record_counter = 1;  /* The first record is going to be added to this block, so set its record_counter to 1 */
    }
  }
  Record *rec = (Record *)malloc(sizeof(Record));  /* Record pointer for memory allocation and management */
  if (rec == NULL) {  /* If a problem occured */
  	CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
    BF_Block_Destroy(&block);  /* Free the previously allocated memory */
    return HP_ERROR;  /* Allocation failed */
  }
  *rec = record;  /* Set the pointer's content as the record that is going to be added */
  memcpy(data, rec, sizeof(Record));  /* Copy that to the record's space of the block */
  free(rec);  /* Free the allocated memory */
  BF_Block_SetDirty(block);  /* Changed data so set block dirty*/
  CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
  BF_Block_Destroy(&block);  /* Free the previously allocated memory */
  return HP_OK;  /* Success */
}

HP_ErrorCode HP_PrintAllEntries(int fileDesc, char *attrName, void* value) {
  int blocks_num;  /* Number of blocks in the open file */
  CALL_BF(BF_GetBlockCounter(fileDesc, &blocks_num));  /* Get that number using BF level */
  if (blocks_num == 0)  /* If there are not any blocks */
    return HP_ERROR;  /* This file has never been created */
  else if (blocks_num == 1)  /* Else if there is only one block */
    return HP_OK;  /* Fine, but there are not any records to print */
  BF_Block *block;  /* Block pointer for memory allocation and management */
  BF_Block_Init(&block);  /* Allocate and initialize memory for a block */
  int block_index = 1;  /* Skip first block (zero indexing), there are not any records there */
  if (value == NULL) {  /* This means that all records need to be printed regardless */
	while (block_index <= blocks_num - 1) {  /* Repeat until last block of the file*/
      CALL_BF(BF_GetBlock(fileDesc, block_index, block));  /* Get current block using BF level */
      int *record_counter = (int *)BF_Block_GetData(block);  /* First data of this block is the record_counter */
      Record *data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
      int record_index = 0;  /* Starting from the first record in this block */
	  while (record_index <= (*record_counter) - 1) {  /* Repeat until last record in this block */
      	Record *record = data + record_index;  /* Go to the current record in this block */
      	printf("%d,\"%s\",\"%s\",\"%s\"\n", record->id, record->name, record->surname, record->city);  /* Print this record */
	    record_index++;  /* Go to next record in this block */
	  }
	  CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
	  block_index++;  /* Go to next block */
    }
  }
  else {  /* Only the records with value in field with name attrName need to be printed */
  	if (strcmp(attrName, "id") == 0) {  /* The field to check is id */
	  while (block_index <= blocks_num - 1) {  /* Repeat until last block of the file*/
		CALL_BF(BF_GetBlock(fileDesc, block_index, block));  /* Get current block using BF level */
		int *record_counter = (int *)BF_Block_GetData(block);  /* First data of this block is the record_counter */
		Record *data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
		int record_index = 0;  /* Starting from the first record in this block */
		while (record_index <= (*record_counter) - 1) {  /* Repeat until last record in this block */
		  Record *record = data + record_index;  /* Go to the current record in this block */
		  if (record->id == *((int *)value))  /* If its id is the requested one */
			printf("%d,\"%s\",\"%s\",\"%s\"\n", record->id, record->name, record->surname, record->city);  /* Print this record */
		  record_index++;  /* Go to next record in the block */
		}
		CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
		block_index++;  /* Go to next block */
	  }
	}
	else if (strcmp(attrName, "name") == 0) {  /* The field to check is name */
  	  while (block_index <= blocks_num - 1) {  /* Repeat until last block of the file*/
		CALL_BF(BF_GetBlock(fileDesc, block_index, block));  /* Get current block using BF level */
		int *record_counter = (int *)BF_Block_GetData(block);  /* First data of this block is the record_counter */
		Record *data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
		int record_index = 0;  /* Starting from the first record in this block */
		while (record_index <= (*record_counter) - 1) {  /* Repeat until last record in this block */
		  Record *record = data + record_index;  /* Go to the current record in this block */
		  if (strcmp(record->name, (char *)value) == 0)  /* If its name is the requested one */
			printf("%d,\"%s\",\"%s\",\"%s\"\n", record->id, record->name, record->surname, record->city);  /* Print this record */
		  record_index++;  /* Go to next record in the block */
		}
		CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
		block_index++;  /* Go to next block */
	  }
	}
	else if (strcmp(attrName, "surname") == 0) {  /* The field to check is surname */
  	  while (block_index <= blocks_num - 1) {  /* Repeat until last block of the file*/
		CALL_BF(BF_GetBlock(fileDesc, block_index, block));  /* Get current block using BF level */
		int *record_counter = (int *)BF_Block_GetData(block);  /* First data of this block is the record_counter */
		Record *data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
		int record_index = 0;  /* Starting from the first record in this block */
		while (record_index <= (*record_counter) - 1) {  /* Repeat until last record in this block */
		  Record *record = data + record_index;  /* Go to the current record in this block */
		  if (strcmp(record->surname, (char *)value) == 0)  /* If its surname is the requested one */
			printf("%d,\"%s\",\"%s\",\"%s\"\n", record->id, record->name, record->surname, record->city);  /* Print this record */
		  record_index++;  /* Go to next record in the block */
		}
		CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
		block_index++;  /* Go to next block */
	  }
	}
	else if (strcmp(attrName, "city") == 0) {  /* The field to check is city */
  	  while (block_index <= blocks_num - 1) {  /* Repeat until last block of the file*/
		CALL_BF(BF_GetBlock(fileDesc, block_index, block));  /* Get current block using BF level */
		int *record_counter = (int *)BF_Block_GetData(block);  /* First data of this block is the record_counter */
		Record *data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
		int record_index = 0;  /* Starting from the first record in this block */
		while (record_index <= (*record_counter) - 1) {  /* Repeat until last record in this block */
		  Record *record = data + record_index;  /* Go to the current record in this block */
		  if (strcmp(record->city, (char *)value) == 0)  /* If its surname is the requested one */
			printf("%d,\"%s\",\"%s\",\"%s\"\n", record->id, record->name, record->surname, record->city);  /* Print this record */
		  record_index++;  /* Go to next record in the block */
		}
		CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
		block_index++;  /* Go to next block */
	  }
	}
	else {  /* attrName is not a valid name of a field */
	  BF_Block_Destroy(&block);  /* Free allocated memory */
	  return HP_ERROR;  /* Invalid input */
	}
  }
  BF_Block_Destroy(&block);  /* Free allocated memory */
  return HP_OK;  /* Success */
}

HP_ErrorCode HP_GetEntry(int fileDesc, int rowId, Record *record) {
  int records_per_block = (BF_BLOCK_SIZE - sizeof(int)) / sizeof(Record);  /* Calculate the maximum number of records that can fit in one block */
  int target_block_index = (rowId - 1) / records_per_block;  /* Find in which block the requested record would be */
  int blocks_num;  /* Number of blocks in the open file */
  CALL_BF(BF_GetBlockCounter(fileDesc, &blocks_num));  /* Get that number using BF level */
  if (blocks_num == 0 || target_block_index > blocks_num - 1)  /* If there are not any blocks or the target block would be out of the file's range */
    return HP_ERROR;  /* The requested record cannot be found in this file */
  BF_Block *block;  /* Block pointer for memory allocation and management */
  BF_Block_Init(&block);  /* Allocate and initialize memory for a block */
  CALL_BF(BF_GetBlock(fileDesc, target_block_index, block));  /* Get target block using BF level */  
  int *record_counter = (int *)BF_Block_GetData(block);  /* First data of target block is the record_counter */
  int target_record_index = (rowId - 1) % records_per_block;  /* Find the position of the target record in this block */
  if (target_record_index > (*record_counter) - 1) {  /* If the position is out of block's range */
  	CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
	BF_Block_Destroy(&block);  /* Free allocated memory */
	return HP_ERROR;  /* The requested record cannot be found in this block */
  }
  Record *data = (Record *)(record_counter + 1);  /* Skip record_counter to get to the record's space */
  *record = *(data + target_record_index);  /* Go to target record and update the content of the returning pointer */
  CALL_BF(BF_UnpinBlock(block));  /* Unpin block */
  BF_Block_Destroy(&block);  /* Free allocated memory */
  return HP_OK;  /* Success */
}

