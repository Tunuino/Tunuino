#pragma once
#ifdef Q_OS_WIN
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <usbiodef.h>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QtEndian>
#include <QString>
#include <functional>

// STM32 ROM DfuSe, Windows WinUSB. This routine is deliberately synchronous.
// No erase is sent until the complete sector backup matches a fresh ECU read.
namespace TunuinoDfu {
using Log = std::function<void(const QString &)>;
inline bool flash(const QByteArray &image, const QByteArray &backup, const Log &log) {
    constexpr quint32 base = 0x08000000u;
    constexpr int blockSize = 2048;
    constexpr quint32 ends[] = {16384,32768,49152,65536,131072,262144,
        393216,524288,655360,786432,917504,1048576};
    if (image.size() < 8 || image.size() > 1048576) return false;
    const uchar *v = reinterpret_cast<const uchar *>(image.constData());
    const quint32 sp = qFromLittleEndian<quint32>(v);
    const quint32 pc = qFromLittleEndian<quint32>(v+4);
    if (sp < 0x20000000u || sp >= 0x20030000u || (sp & 7u)
        || !(pc & 1u) || (pc & ~1u) < base || (pc & ~1u) >= base + quint32(image.size())) {
        log(QStringLiteral("BLOCKED: invalid firmware vector table.")); return false;
    }
    int count = 0;
    while (count < 12 && ends[count] < quint32(image.size())) ++count;
    if (count == 12) return false;
    ++count;
    if (backup.size() != int(ends[count-1])) {
        log(QStringLiteral("BLOCKED: full erase-footprint backup required.")); return false;
    }
    const GUID guid = {0xA5DCBF10,0x6530,0x11D2,{0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED}};
    HDEVINFO devices = SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (devices == INVALID_HANDLE_VALUE) { log(QStringLiteral("WinUSB enumeration failed.")); return false; }
    HANDLE handle = INVALID_HANDLE_VALUE;
    int matches = 0;
    SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
    for (DWORD i=0; SetupDiEnumDeviceInterfaces(devices,nullptr,&guid,i,&iface);++i) {
        DWORD len=0; SetupDiGetDeviceInterfaceDetailW(devices,&iface,nullptr,0,&len,nullptr);
        if (!len) continue;
        QByteArray mem(int(len),char(0));
        auto *detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(mem.data());
        detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(devices,&iface,detail,len,nullptr,nullptr)) continue;
        const QString path=QString::fromWCharArray(detail->DevicePath).toLower();
        if (!path.contains(QStringLiteral("vid_0483")) || !path.contains(QStringLiteral("pid_df11"))) continue;
        ++matches;
        if (matches == 1) handle=CreateFileW(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,nullptr);
    }
    SetupDiDestroyDeviceInfoList(devices);
    if (matches != 1 || handle == INVALID_HANDLE_VALUE) {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        log(QStringLiteral("BLOCKED: exactly one accessible STM32 DFU device is required.")); return false;
    }
    WINUSB_INTERFACE_HANDLE usb=nullptr;
    if (!WinUsb_Initialize(handle,&usb)) { CloseHandle(handle); log(QStringLiteral("WinUSB initialization failed.")); return false; }
    const auto cleanup=[&]() { WinUsb_Free(usb); CloseHandle(handle); };
    const auto control=[&](UCHAR type,UCHAR req,USHORT value,UCHAR *data,USHORT len)->bool {
        WINUSB_SETUP_PACKET p{};p.RequestType=type;p.Request=req;p.Value=value;p.Index=0;p.Length=len;
        ULONG actual=0;UCHAR dummy=0;
        if (!WinUsb_ControlTransfer(usb,p,len?data:&dummy,len,&actual,nullptr)) return false;
        return actual==len;
    };
    const auto status=[&](UCHAR &state,UCHAR &error,unsigned &wait)->bool {
        UCHAR data[6]{};
        if (!control(0xA1,3,0,data,6)) return false;
        error=data[0];state=data[4];
        wait=unsigned(data[1])|(unsigned(data[2])<<8)|(unsigned(data[3])<<16);
        return true;
    };
    const auto settle=[&]()->bool {
        for(int i=0;i<120;++i) {
            UCHAR state=0,error=0;unsigned wait=0;
            if (!status(state,error,wait) || error!=0 || state==10) return false;
            if (state==5) return true;
            if (state!=3 && state!=4) return false;
            if (wait>10000) return false;
            Sleep(wait?wait:10);
        }
        return false;
    };
    const auto idle=[&]()->bool {
        UCHAR state=0,error=0;unsigned wait=0;
        if (!status(state,error,wait)) return false;
        if (state==10) {
            if (!control(0x21,4,0,nullptr,0) || !status(state,error,wait)) return false;
        }
        if (error!=0) return false;
        if (state!=2) {
            if (!control(0x21,6,0,nullptr,0) || !status(state,error,wait)) return false;
        }
        return state==2 && error==0;
    };
    const auto command=[&](UCHAR op,quint32 address)->bool {
        UCHAR data[5]={op,UCHAR(address),UCHAR(address>>8),UCHAR(address>>16),UCHAR(address>>24)};
        return control(0x21,1,0,data,5) && settle();
    };
    const auto read=[&](int size,QByteArray &out)->bool {
        if (!idle() || !command(0x21,base) || !idle()) return false;
        out.clear();out.reserve(size);
        for(int offset=0;offset<size;offset+=blockSize) {
            const int n=qMin(blockSize,size-offset);
            QByteArray chunk(n,char(0));
            if (!control(0xA1,2,USHORT(2+offset/blockSize),
                    reinterpret_cast<UCHAR *>(chunk.data()),USHORT(n))) return false;
            out.append(chunk);
        }
        return idle();
    };
    USB_DEVICE_DESCRIPTOR desc{};ULONG received=0;
    if (!WinUsb_GetDescriptor(usb,USB_DEVICE_DESCRIPTOR_TYPE,0,0,
        reinterpret_cast<PUCHAR>(&desc),sizeof(desc),&received)
        || received!=sizeof(desc) || desc.idVendor!=0x0483 || desc.idProduct!=0xDF11) {
        log(QStringLiteral("BLOCKED: USB identity mismatch."));cleanup();return false;
    }
    // Fail closed unless the expected 1 MiB STM32 Flash map is advertised.
    UCHAR config[512]{};received=0;
    if (!WinUsb_GetDescriptor(usb,USB_CONFIGURATION_DESCRIPTOR_TYPE,0,0,config,sizeof(config),&received)
        || received<9) { log(QStringLiteral("BLOCKED: DFU descriptors unavailable."));cleanup();return false; }
    bool dfu=false;
    for(ULONG p=0;p+2<=received && config[p]>=2 && p+config[p]<=received;p+=config[p]) {
        if (config[p]==9 && config[p+1]==4 && config[p+5]==0xfe && config[p+6]==1 && config[p+7]==2) dfu=true;
    }
    if (!dfu) {log(QStringLiteral("BLOCKED: incompatible DFU interface."));cleanup();return false;}
    QByteArray live;
    log(QStringLiteral("Reading live ECU Flash before erase..."));
    if (!read(backup.size(),live) || live!=backup) {
        log(QStringLiteral("BLOCKED: live Flash does not match saved sector backup. No erase attempted."));
        cleanup();return false;
    }
    log(QStringLiteral("Live backup comparison PASS. Beginning sector erase."));
    bool ok=idle();
    for(int i=0;ok && i<count;++i) {
        const quint32 address=base+(i?ends[i-1]:0);
        log(QStringLiteral("Erasing sector %1 at 0x%2").arg(i).arg(address,8,16,QLatin1Char('0')));
        ok=command(0x41,address);
    }
    if (ok) ok=command(0x21,base);
    for(int offset=0;ok && offset<image.size();offset+=blockSize) {
        const int n=qMin(blockSize,image.size()-offset);
        QByteArray data=image.mid(offset,n);
        ok=control(0x21,1,USHORT(2+offset/blockSize),
            reinterpret_cast<UCHAR *>(data.data()),USHORT(n)) && settle();
        if (ok) log(QStringLiteral("Programmed %1 / %2 bytes").arg(offset+n).arg(image.size()));
    }
    if (!ok) {
        log(QStringLiteral("FLASH FAILED during erase/program. ECU may require recovery in DFU mode."));
        cleanup();return false;
    }
    QByteArray verify;
    log(QStringLiteral("Programming complete. Reading back firmware..."));
    ok=read(image.size(),verify) && verify==image;
    log(ok ? QStringLiteral("FLASH SUCCESS: full byte-for-byte readback PASS.")
           : QStringLiteral("FLASH VERIFY FAILED: do not run ECU; recover via DFU."));
    cleanup();return ok;
}
} // namespace TunuinoDfu
#endif
