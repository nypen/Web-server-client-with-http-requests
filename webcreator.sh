#!/bin/bash
#./webcreator.sh root_directory text_file w p
#

already_exists(){
  #returns 1 if the number exists
  #returns 0 if it doesnt
  #already_exists new_number , array

  local new_num=$1
  shift
  local p
  local arr=("$@")
  for p in "${arr[@]}"  #check if it exists already
    do
    if [ $p -eq $new_num ]; then    #if the same random number exists break
      no_same=1
      break
    fi
  done #for
}

wrong_arguments=0
if [ "$(ls -A $1)" ]
then
  echo "#Purging.."
  `rm -r $1`
  `mkdir $1`
fi

#if given Directory does not exist inform and exit
if ! [ -d $1 ]
  then
  wrong_arguments=1
  echo "Directory $1 does not exist. Try again."
  exit 99
fi

#if given file does not exist inform and exit
if [ ! -e $2 ]
  then
  wrong_arguments=1
  echo "File $2 does not exist. Try again."
#  exit 99
fi


#if given numbers are not integers inform and exit
if ! [[ "$3" =~ ^[0-9]+$ ]]
  then
  wrong_arguments=1
  echo "W must be an integer"
fi

if ! [[ "$4" =~ ^[0-9]+$ ]]
  then
  echo "P must be an integer"
fi

min=10
#store bytes of file
size=`wc -c my.txt|cut -f1 -d' '`
if [ $size -le $min ];
  then
  wrong_arguments=1
  echo "File size is not valid.Characters must be more than 10.000"
fi

if [ $wrong_arguments == 1 ]; then
  echo "Bash exits.. Try again."
  exit 2
fi
dir=$1
filename=$2
W=$3
P=$4
counter=0
for ((i=0; i < W ; i++)) do
  temp_dirname="$dir/site$i"
  mkdir $temp_dirname
  for ((j=0; j < P ; j++)) do
    #pick a 4digit random number
    no_same=1
    if [ $j -gt 0 ];then  #if its the second or greater random number check if it exists again
      while [ $no_same -eq 1 ]
        do
          random_num=$(( ( RANDOM % 10000 )  + 1000 ))
          no_same=0
          already_exists $random_num ${randoms[@]}
          if [ $no_same -eq 1 ];then
            break
          fi
        done #while
      randoms[$j]=$random_num
    else #if it is the first random number just place it
      random_num=$(( ( RANDOM % 10000 )  + 1000 ))
      randoms[$j]=$random_num
    fi
  done
  #creating the pages in each site,store pages' names and write the standard html code
  for ((j=0; j < P ; j++)) do
    temp_filename="$temp_dirname/page"$i"_${randoms[$j]}.html"
    touch $temp_filename
    echo "<!DOCTYPE html>" >> $temp_filename
    echo -e "<html> \n\t <body>" >> $temp_filename
    temp_filename="site$i/page"$i"_${randoms[$j]}.html"
    pagenames[$counter]="${temp_filename}"
    counter=$counter+1

  done
done

#pick a random k, 1<k<text_file -2000
lines=`wc -l my.txt|cut -f1 -d' '`
lines=`expr $lines - 2000`
k_var=$(( ( RANDOM % $lines )  + 2 ))
m=$(( ( RANDOM % 1000 )  + 1000 ))
f=$(($P/2))
f=$(($f+1))
q=$(($W/2))
q=$(($q+1))

total_pages=`expr $W \* $P`
tmp=0
for ((i=0; i < total_pages; i++)) do
  incoming_link[$i]=0
done

#tmp=`expr $P \* $i`  to i einai to site pou koitaw kathe fora
for ((i=0; i < W; i++)) do
  echo " "
  echo "#Creating website $i"
  for ((j=0; j < P; j++)) do
    tmp1=$(($P*$i))
    for ((k=0; k < f; k++)) do #find f internal links
      if [ $k -gt 0 ];then
        no_same=1
        while [ $no_same -eq 1 ]
        do
          no_same=0
          random_num=$(( ( RANDOM % $P )  + $tmp1 )) #pick a random number between P*i and P*i+1
          page_num=$(($i*$P))
          page_num=$(($page_num+$j))
          if [ $random_num -eq  $page_num ]; then  #If the number is equal to this page's number ,find another
            no_same=1
            continue
          fi
          no_same=0
          already_exists $random_num ${links[@]}
        done #while
        links[$k]=$random_num
        incoming_link[$random_num]=1
      else #if it is the first random number just place it
        random_num=$(( ( RANDOM % $P )  + $tmp1 )) #pick a random number between P*i and P*i+1
        while [ $random_num -eq $i ]; do  #If the number is equal to this page's number ,find another
          random_num=$(( ( RANDOM % $P )  + $tmp1 )) #pick a random number between P*i and P*i+1
        done
        links[$k]=$random_num
        incoming_link[$random_num]=1
      fi
    done
    for ((k=f; k < f+q; k++)) do
      no_same=1
      while [ $no_same -eq 1 ]
      do
        no_same=0
        random_num=$(( RANDOM % $total_pages  )) #pick a random number from all pages
        #if random link is one of the internal links pick another
        if [ $random_num -ge $tmp1 ] && [ $random_num -lt $(($tmp1+$P)) ];then
          no_same=1
          continue
        fi
        already_exists $random_num ${links[@]}
      done
      links[$k]=$random_num
      incoming_link[$random_num]=1

    done
    #array links is now filled with numbers from 0-W*P
    #we will print the m(f+q) lines and one of the links
    no_lines=$(($f+$q))
    no_lines=`expr $m / $no_lines`
    temp_filename="$1/${pagenames[$page_num]}"
    k_var=$(( ( RANDOM % $lines )  + 2 ))
    tmp3=$(($f+$q))
    tmp4=$(($no_lines * $tmp3))
    echo "# Creating page $temp_filename , with $m lines,starting from line $k_var ."
    for ((k=0; k < f+q; k++)) do  #for every link
      sed -n "$k_var,+${no_lines}p" $filename >> $temp_filename
      k_var=$(($k_var+$no_lines))
      tmp=${links[$k]}
      echo "# Adding link to $temp_filename.."
      link="${pagenames[$tmp]}"
      echo "<a href=\"../$link\"> link$k</a> " >>  $temp_filename

    done
    for ((k=0; k < f+q; k++)) do
      links[$k]=-1
    done

  done
done

for ((i=0; i < total_pages; i++)) do
  echo "\t</body>\n</html>" >> "$1/${pagenames[$i]}"
done

inc=0
for ((i=0; i < total_pages; i++)) do
    if [ ${incoming_link[$i]} -eq 0 ];then
      inc = 1
      break
    fi
done

if [ $inc -eq 0 ]; then
  echo "# Every page has at least one incoming link"
else
  echo "There are pages without incoming links"
fi

echo "# Done "
