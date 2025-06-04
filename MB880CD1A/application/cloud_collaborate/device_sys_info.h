#ifndef _DEVICE_SYS_INFO_H
#define _DEVICE_SYS_INFO_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */


void DeviceSystemInfoGetInit(void);
//void DeviceSystemInfoGetEntry(void);
char* FirmwareVersionGet();
char* CpuFrequencyGet();
char* CpuTemperatureGet();
char* PowerUsageGet();
char* ComponentUpgradeCmdGet();
char* ServiceMonitorCmdsGet();
char* AllSystemInfoGet();

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _DEVICE_SYS_INFO_H */


