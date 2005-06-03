#!/bin/bash

while read x ;
do
    echo $x;
    mailx -s "wavbreaker release 0.6.1" "${x}" < message.txt;
done < wavbreaker-announce.list
#done < testlist.list

