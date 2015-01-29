# include "Function.h"
#pragma warning(disable:4127)//����4227����


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
		//��ȡFileNameǰ��Ĳ���(�豸·�����Ŀ¼·��)
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
		//ʧ��ֱ������
		if (!NT_SUCCESS(status))
		{
			break;
		}

		//�ж϶���֮���ǹ���Ҫ��һ��б�ܡ�����������
		//1��FIleName��һ���ַ�����б��
		//2��pObj_name_info���һ���ַ�����б��
		if (file->FileName.Length > 2 &&
			file->FileName.Buffer[0] != L'\\' &&
			pObj_name_info->Name.Buffer[pObj_name_info->Name.Length / sizeof(WCHAR) - 1] != L'\\')
		{
			need_split = TRUE;
		}

		//�����������ֵĳ��ȡ�������Ȳ��㣬Ҳֱ�ӷ��ء�
		length = pObj_name_info->Name.Length + file->FileName.Length;
		if (need_split == TRUE)
		{
			length += sizeof(WCHAR);
		}
		if (path->MaximumLength < length)
		{
			break;
		}

		//�Ȱ��豸��������ȥ
		RtlCopyUnicodeString(path, &file->FileName);
		if (need_split == TRUE)
		{
			//׷��һ��б��
			RtlAppendUnicodeToString(path, L"\\");
		}

		//���׷��FileName
		RtlAppendUnicodeStringToString(path, &file->FileName);

	} while (FALSE);

	//���������ռ䣬���ͷŵ�
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

	//���������ļ��Ĵ�СΪFUNC_FILE_HEADER_SIZE
	status = SetFileSize(next_dev, file, &file_size);
	if (status != STATUS_SUCCESS)
	{
		return status;
	}

	//Ȼ��д��8���ֽڵ�ͷabcd
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

	//����irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//��дirp����
	irp->AssociatedIrp.SystemBuffer = buf;
	irp->UserEvent = &event;
	irp->UserIosb = &ioStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread(); 
	irp->Tail.Overlay.OriginalFileObject = file;
	irp->RequestorMode = KernelMode;
	irp->Flags = 0;

	//����irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = IRP_MJ_SET_INFORMATION;
	ioStackLocation->DeviceObject = dev;
	ioStackLocation->FileObject = file;
	ioStackLocation->Parameters.SetFile.FileObject = set_file;
	ioStackLocation->Parameters.SetFile.Length = buf_Len;
	ioStackLocation->Parameters.SetFile.FileInformationClass = info_class;

	//���ý�������
	IoSetCompletionRoutine(
		irp, FileIrpComp, 
		0, TRUE, TRUE, TRUE);

	//�������󲢵ȴ�
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

	//����irp
	irp = IoAllocateIrp(dev->StackSize, FALSE);
	if (irp == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//��д����
	irp->AssociatedIrp.SystemBuffer = NULL;
	//��paging io������£��ƺ�����Ҫʹ��MDL�����������С�
	//����ʹ��UserBuffer�������Ҳ����϶���һ�㡣���������һ�����ԣ��Ա���ٴ���
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

	//��дirpsp
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

	//�������
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
	//���Ȼ��Ҫ�򿪵��ļ���·��
	ULONG length = GetFileFullPathName(file, &path);

	ULONG ret = SF_IRP_PASS;
	PFILE_OBJECT my_file = NULL;
	HANDLE file_h;
	ULONG information = 0;
	LARGE_INTEGER file_size, offset = { 0 };
	BOOLEAN dir, sec_file;

	//��ô򿪷���Ȩ��
	ACCESS_MASK desired_access = irpsp->Parameters.Create.SecurityContext->DesiredAccess;
	WCHAR header_flags[4] = { L'A', L'B', L'C', L'D' };
	WCHAR header_buf[4] = { 0 };
	ULONG disp;

	//�޷���� ·����ֱ�ӷŹ�
	if (irpsp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
	{
		return SF_IRP_PASS;
	}

	do{
		//��path���仺����
		path.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, length + 4, FUNC_MEM_TAG);
		path.Length = 0;
		path.MaximumLength = (USHORT)length + 4;
		if (path.Buffer == NULL)
		{
			//�ڴ治�����������ֱ�ӹҵ�
			status = STATUS_INSUFFICIENT_RESOURCES;
			ret = SF_IRP_COMPLETED;
			break;
		}
		length = GetFileFullPathName(file, &path);

		//�õ�·����������ļ�
		file_h = CreateFileAccordingIrp(
			next_dev,
			&path,
			irpsp,
			&status,
			&my_file,
			&information);

		//���û�гɹ��򿪣���ô˵�����������Խ����ˡ�
		if (!NT_SUCCESS(status))
		{
			ret = SF_IRP_COMPLETED;
			break;
		}

		//�õ�my_file֮�������ж�����ļ��ǲ����Ѿ��ڼ��ܵ��ļ�֮�У�����ڣ�ֱ�ӷ���passtrue����



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
	//��дobject attribute
	InitializeObjectAttributes(
		&obj_attri,
		file_full_path,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

	//����IoCreateFileSpecifyDeviceObjectHint���ļ�
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

	//��סinformation�������������
	*information = io_status.Information;

	//�Ӿ���ĵ�һ��fileobject���ں���Ĳ������ǵ�һ��Ҫ�������
	*status = ObReferenceObjectByHandle(
		file_h,
		0,
		*IoFileObjectType,
		KernelMode,
		(PVOID *)file,
		NULL);

	//���ʧ���˾͹رգ�����û�д��ļ�������ʵ�����ǲ�Ӧ�ó��ֵġ�
	if (!NT_SUCCESS(*status))
	{
		ASSERT(FALSE);
		ZwClose(file_h);
	}

	return file_h;
}