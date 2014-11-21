BEGIN{
}

# if it does not begin with M, which is the movement information
$1!~/M/{ 

# only display four number after the point for better viewing of the trace file
$3=sprintf("%.4f", $3) 

# delete the -Nx, -Ny, -Nz and -Ne information 
$10=""
$11=""
$12=""
$13=""
$14=""
$15=""
$16=""
$17=""
$34=""
$35=""
$36=""
$37=""

# because we usually delete two parameters, substitute 
gsub("  ","",$0)

print $0
}

END{

}
