module likwid

interface 

  subroutine likwid_markerInit()
  end subroutine likwid_markerInit

  subroutine likwid_markerClose()
  end subroutine likwid_markerClose

  subroutine likwid_markerStartRegion( regionTag, strLen )
  character(*) :: regionTag
  integer strLen
  end subroutine likwid_markerStartRegion

  subroutine likwid_markerStopRegion( regionTag, strLen )
  character(*) :: regionTag
  integer strLen
  end subroutine likwid_markerStopRegion

end interface

end module likwid

