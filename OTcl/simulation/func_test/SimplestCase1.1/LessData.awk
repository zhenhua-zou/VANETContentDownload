BEGIN{
}

$1~/M/ {}

$1!~/M/ && ($1~/r/||$1~/s/){
$4=""
$5=""
$6=""
$7=""
$10=""
$11=""
$12=""
$13=""
$14=""
$15=""
$16=""
$17=""
$22=""
$23=""
$24=""
$25=""
$26=""
$27=""
$28=""
$29=""
$38=""
$39=""
$40=""
$41=""
$42=""
$43=""
$48=""
$49=""
$50=""
$51=""

gsub("  ","",$0)
print $0

}

END{

}
