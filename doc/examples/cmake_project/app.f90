program app
use iso_c_binding
implicit none
include 'dlbf_talp.h'
type(c_ptr) :: dlb_handle
integer :: err

dlb_handle = DLB_MonitoringRegionRegister(c_char_"Region 1"//C_NULL_CHAR)


! Resume region
err = DLB_MonitoringRegionStart(dlb_handle)

! Pause region
err = DLB_MonitoringRegionStop(dlb_handle)

end program app