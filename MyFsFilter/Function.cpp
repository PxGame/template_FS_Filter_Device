# include "Function.h"
#pragma warning(disable:4127)//忽略4227警告


#define FUNC_FILE_HEADER_SIZE (1024*4)
#define FUNC_MEM_TAG 'Tag1'

typedef enum{
	SF_IRP_GO_ON = 0,
	SF_IRP_COMPLETED = 1,
	SF_IRP_PASS = 2
} SF_RET;

ULONG 
GetFileFullPathName(
	_In_ PFILE_OBJECT file,
	_Out_ PUNICODE_STRING path
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	POBJECT_NAME_INFORMATION pObj_name_info = NULL;
	WCHAR buf[MAXIMUM_FILENAME_LENGTH] = { 0 };
	PVOID pObj = NULL;
	ULONG length = 0;
	BOOLEAN need_split = FALSE;

	ASSERT(file != NULL);
	if (file == NULL)
	{
		return 0;
	}
	if (file->FileName.Buffer == NULL)
	{
		return 0;
	}

	pObj_name_info = (POBJECT_NAME_INFORMATION)buf;
	do{
		//获取FileName前面的部分(设备路径或根目录路径)
		if (file->RelatedFileObject != NULL)
		{
			pObj = (PVOID)file->RelatedFileObject;
		}
		else
		{
			pObj = (PVOID)file->DeviceObject;
		}
		status = ObQueryNameString(
			pObj,
			pObj_name_info,
			MAXIMUM_FILENAME_LENGTH * sizeof(WCHAR),
			&length
			);
		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			pObj_name_info = 
				(POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(
																											NonPagedPool,
																											length,
																											FUNC_MEM_TAG);
			if (pObj_name_info == NULL)
			{
				return 0;
			}
			RtlZeroMemory(pObj_name_info, length);
			status = ObQueryNameString(
				pObj,
				pObj_name_info,
				length,
				&length
				);
		}
		//失败直接跳出
		if (!NT_SUCCESS(status))
		{
			break;
		}

		//判断二者之间是够需要多一个斜杠。有两个条件
		//1、FIleName第一个字符不是斜杠
		//2、pObj_name_info最后一个字符不是斜杠
		if (file->FileName.Length > 2 &&
			file->FileName.Buffer[0] != L'\\' &&
			pObj_name_info->Name.Buffer[pObj_name_info->Name.Length / sizeof(WCHAR) - 1] != L'\\')
		{
			need_split = TRUE;
		}

		//计算总体名字的长度。如果长度不足，也直接返回。
		length = pObj_name_info->Name.Length + file->FileName.Length;
		if (need_split == TRUE)
		{
			length += sizeof(WCHAR);
		}
		if (path->MaximumLength < length)
		{
			break;
		}

		//先把设备名拷贝进去
		RtlCopyUnicodeString(path, &file->FileName);
		if (need_split == TRUE)
		{
			//追加一个斜杠
			RtlAppendUnicodeToString(path, L"\\");
		}

		//最后追加FileName
		RtlAppendUnicodeStringToString(path, &file->FileName);

	} while (FALSE);

	//如果分配过空间，就释放掉
	if ((PVOID)pObj_name_info != (PVOID)buf)
	{
		ExFreePool(pObj_name_info);
	}

	return length;
}


NTSTATUS 
WriteFileHeader(
	_In_ PFILE_OBJECT file,
	_In_ PDEVICE_OBJECT next_dev)
{
	NTSTATUS status = FALSE;
	static WCHAR header_flags[FUNC_FILE_HEADER_SIZE / sizeof(WCHAR)] = {L'A',L'B',L'C',L'D'};
	LARGE_INTEGER file_size, offset;
	ULONG length = FUNC_FILE_HEADER_SIZE;
	
	offset.QuadPart = 0;
	file_size.QuadPart = FUNC_FILE_HEADER_SIZE;

	//首先设置文件的大小为FUNC_FILE_HEADER_SIZE
	status = SetFileSize(next_dev, file, &file_size);
	if (status != STATUS_SUCCESS)
	{
		return status;
	}

	//然后写入8个字节的头abcd
	return ReadWriteFile(
		next_dev, file, &offset, &length, header_flags, FALSE);
}


NTSTATUS 
SetFileSize(
	_In_ PDEVICE_OBJECT dev,
	_In_ PFILE_OBJECT file,
	_In_ PLARGE_INTEGER file_size)
{
	FILE_END_OF_FILE_INFORMATION end_of_file;
	end_of_file.EndOfFile.QuadPart = file_size->QuadPart;

	return SetFileInformation(
		dev, file, FileEndOfFileInformation,
		NULL, (PVOID)&end_of_file,
		sizeof(FILE_END_OF_FILE_INFORMATION));
}

NTSTATUS
SetFileInformation(
_In_ PDEVICE_OBJECT dev,
_In_ PFILE_OBJECT file,
_In_ FILE_INFORMATION_CLASS info_class,
_In_ PFILE_OBJECT set_file,
_In_ PVOID buf,
_In_ ULONG buf_Len)
{
	PIRP irp;
	KEVENT event;
	IO_STATUS_BLOCK ioStatusBlock;
	PIO_STACK_LOCATION ioStackLocation;

	KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	//分配irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//填写irp主体
	irp->AssociatedIrp.SystemBuffer = buf;
	irp->UserEvent = &event;
	irp->UserIosb = &ioStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread(); 
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	irp->Flags = 0;

	//设置irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = IRP_MJ_SET_INFORMATION;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	ioStackLocation->Parameters.SetFile.FileObject = set_file;
	ioStackLocation->Parameters.SetFile.Length = buf_Len;
	ioStackLocation->Parameters.SetFile.FileInformationClass = info_class;

	//设置结束例程
	IoSetCompletionRoutine(
		irp, FileIrpComp, 
		0, TRUE, TRUE, TRUE);

	//发送请求并等待
	(void)IoCallDriver(dev, irp);
	KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	return ioStatusBlock.Status;
}


NTSTATUS FileIrpComp(
	_In_ PDEVICE_OBJECT dev,
	_In_ PIRP irp,
	PVOID context
	)
{
	UNREFERENCED_PARAMETER(dev);
	UNREFERENCED_PARAMETER(context);

	*irp->UserIosb = irp->IoStatus;
	KeSetEvent(irp->UserEvent, 0, FALSE);
	IoFreeIrp(irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS ReadWriteFile(
	_In_ PDEVICE_OBJECT dev,
	_In_ PFILE_OBJECT file,
	_In_ PLARGE_INTEGER offset,
	_In_ PULONG length,
	_In_ PVOID buffer,
	_In_ BOOLEAN read_write)
{
	PIRP irp;
	KEVENT event;
	PIO_STACK_LOCATION ioStackLocation;
	IO_STATUS_BLOCK ioStatusBlock = { 0 };

	KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	//分配irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//填写主体
	irp->AssociatedIrp.SystemBuffer = NULL;
	//在paging io的情况下，似乎必须要使用MDL才能正常进行。
	//不能使用UserBuffer，但是我并不肯定这一点。所以这里加一个断言，以便跟踪错误。
	irp->MdlAddress = NULL;
	irp->UserBuffer = buffer;
	irp->UserEvent = &event;
	irp->UserIosb = &ioStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	if (read_write == TRUE)
	{
		irp->Flags = IRP_READ_OPERATION | IRP_DEFER_IO_COMPLETION | IRP_NOCACHE;
	}
	else
	{
		irp->Flags = IRP_WRITE_OPERATION | IRP_DEFER_IO_COMPLETION | IRP_NOCACHE;
	}

	//填写irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	if (read_write == TRUE)
	{
		ioStackLocation->MajorFunction = IRP_MJ_READ;
	}
	else
	{
		ioStackLocation->MajorFunction = IRP_MJ_WRITE;
	}

	ioStackLocation->MinorFunction = IRP_MN_NORMAL;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	if (read_write == TRUE)
	{
		ioStackLocation->Parameters.Read.ByteOffset = *offset;
		ioStackLocation->Parameters.Read.Length = *length;
	}
	else
	{
		ioStackLocation->Parameters.Write.ByteOffset = *offset;
		ioStackLocation->Parameters.Write.Length = *length;
	}

	//设置完成
	IoSetCompletionRoutine(
		irp, FileIrpComp,
		0, TRUE, TRUE, TRUE);
	(void)IoCallDriver(dev, irp);
	KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	*length = ioStatusBlock.Information;

	return ioStatusBlock.Status;
}

ULONG IrpCreatePre(
	_In_ PIRP irp,
	_In_ PIO_STACK_LOCATION irpsp,
	_In_ PFILE_OBJECT file,
	_In_ PDEVICE_OBJECT next_dev)
{
	NTSTATUS status = FALSE; 

	UNICODE_STRING path = { 0 };
	//首先获得要打开的文件的路径
	ULONG length = GetFileFullPathName(file, &path);

	ULONG ret = SF_IRP_PASS;
	PFILE_OBJECT my_file = NULL;
	HANDLE file_h;
	ULONG information = 0;
	LARGE_INTEGER file_size, offset = { 0 };
	BOOLEAN dir, sec_file;

	//获得打开访问权限
	ACCESS_MASK desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
	WCHAR header_flags[4] = { L'A', L'B', L'C', L'D' };
	WCHAR header_buf[4] = { 0 };
	ULONG disp;

	//无法获得 路径，直接放过
	if (irpsp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
	{
		return SF_IRP_PASS;
	}

	do{
		//给path分配缓冲区
		path.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, length + 4, FUNC_MEM_TAG);
		path.Length = 0;
		path.MaximumLength = (USHORT)length + 4;
		if (path.Buffer == NULL)
		{
			//内存不够，这个请求直接挂掉
			status = STATUS_INSUFFICIENT_RESOURCES;
			ret = SF_IRP_COMPLETED;
			break;
		}
		length = GetFileFullPathName(file, &path);

		//得到路径，打开这个文件
		file_h = CreateFileAccordingIrp(
			next_dev,
			&path,
			irpsp,
			&status,
			&my_file,
			&information);

		//如果没有成功打开，那么说明这个请求可以结束了。
		if (!NT_SUCCESS(status))
		{
			ret = SF_IRP_COMPLETED;
			break;
		}

		//得到my_file之后，首先判断这个文件是不是已经在加密的文件之中，如果在，直接返回passtrue即可



	} while (FALSE);


}

HANDLE CreateFileAccordingIrp(
	_In_ PDEVICE_OBJECT dev,
	_In_ PUNICODE_STRING file_full_path,
	_In_ PIO_STACK_LOCATION irpsp,
	_Out_ PNTSTATUS status,
	_Out_ PFILE_OBJECT *file,
	_Out_ PULONG information)
{
	HANDLE file_h = NULL;
	IO_STATUS_BLOCK io_status;
	ULONG desried_access;
	ULONG dispostion;
	ULONG create_options;
	ULONG share_access;
	ULONG file_attri;
	OBJECT_ATTRIBUTES obj_attri;

	ASSERT(irpsp->MajorFunction == IRP_MJ_CREATE);

	*information = 0;
	//填写object attribute
	InitializeObjectAttributes(
		&obj_attri,
		file_full_path,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

	//调用IoCreateFileSpecifyDeviceObjectHint打开文件
	*status = IoCreateFileSpecifyDeviceObjectHint(
		&file_h,
		desried_access,
		&obj_attri,
		&io_status,
		NULL,
		file_attri,
		share_access,
		dispostion,
		create_options,
		NULL,
		0,
		CreateFileTypeNone,
		NULL,
		0,
		dev);
	if (!NT_SUCCESS(*status))
	{
		return file_h;
	}

	//记住information，便于外面调用
	*information = io_status.Information;

	//从句柄的到一个fileobject便于后面的操作。记得一定要解除引用
	*status = ObReferenceObjectByHandle(
		file_h,
		0,
		*IoFileObjectType,
		KernelMode,
		(PVOID *)file,
		NULL);

	//如果失败了就关闭，假设没有打开文件。但这实际上是不应该出现的。
	if (!NT_SUCCESS(*status))
	{
		ASSERT(FALSE);
		ZwClose(file_h);
	}

	return file_h;
}