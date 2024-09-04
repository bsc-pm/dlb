#include <dlb_talp.h>

#include<unistd.h>
int main(){
/* Create a region */
dlb_monitor_t *monitor = DLB_MonitoringRegionRegister("Region 1");
/* Start region */
DLB_MonitoringRegionStart(monitor);
sleep(1);
/* Stop region */
DLB_MonitoringRegionStop(monitor);

}