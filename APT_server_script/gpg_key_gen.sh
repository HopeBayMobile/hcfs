#!/usr/bin/expect

spawn gpg --gen-key
expect "Your selection?"
sleep 0.5
send "1\n"

expect "What keysize do you want? (2048)"
sleep 0.5
send "\n"

expect "Key is valid for? (0)"
sleep 0.5
send "0\n"

expect "Is this correct? (y/N)"
sleep 0.5
send "y\n"

expect "Real name:"
sleep 0.5
send "Delta Cloud\n"

expect "Email address:"
sleep 0.5
send "yen.kuo@delta.com.tw\n"

expect "Comment:"
sleep 0.5
send "Cloud Data Solution Team\n"

expect "Change (N)ame, (C)omment, (E)mail or (O)kay/(Q)uit?"
sleep 0.5
send "O\n"

expect "Enter passphrase:"
sleep 0.5
send "\n"

expect "passphrase:"
sleep 0.5
send "\n"

interact 
