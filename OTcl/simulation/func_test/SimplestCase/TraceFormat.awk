###################################################################################################
# field map
# $1        Event type
# $2,$3     -t : time 
# $4,$5     -Hs: id for this node
# $6,$7     -Hd: id for next hop towards the destination
# $8,$9     -Ni: node id 
# $18,$19   -Nl: trace level, such as AGT, RTR, MAC
# $20,$21   -Nw: reason for the event  
# $30,$31   -Is: source address.source port number 
# $32,$33   -Id: dest address.dest port number 
# $36,$37   -Il: packet size 
###################################################################################################
BEGIN{
  TotalSize=0
  num=0
  StartFlag=0
  StartTime=0
  EndTime=0
}

{
  EventType=$1
  id=$5
  PktSize=$37
  time=$3

  if ($1=="r") {
    num=num+1
    # time information
    if(StartFlag==0) {
      StartFlag=1;
      StartTime=time;
    } else {
       EndTime=time;
    }

    TotalSize=TotalSize+PktSize
  }

}

END{
  print "Total Size is  " TotalSize " bytes"

  Duration=EndTime-StartTime
  print "Duration is " Duration

  printf "rate is %f bps \n",TotalSize*8/Duration
  
  print "Num is " num
}
