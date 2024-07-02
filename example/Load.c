

//
// When the kernel loads, or whenever you are ready to load kd:
//
//        KdKernelBase       = KshShim_Ntoskrnl_ImageBase;
//        KdKernelSize       = (ULONG32)KshShim_Ntoskrnl_ImageSize;
//
//#if LIME_HV_ENABLE_NOKD
//        DbgKdInitStatus = KdDriverLoad();
//        if (K_SUCCESS(DbgKdInitStatus)) {
//            DBG_PRINT("KdDriverLoad success\n");
//
//            DbgKdConnectionStatus = KdTryConnect();
//            if (K_SUCCESS(DbgKdConnectionStatus)) {
//                DBG_PRINT("KdTryConnect success\n");
//            }
//        }
//#endif