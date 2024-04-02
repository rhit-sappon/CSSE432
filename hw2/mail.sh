#!/usr/bin/expect
# Made by Owen Sapp 
# Date of last modification: 3/31/24
set spoof_name "Pirate Steve"
set spoof_email "stevep@dodgeball.org"
set spoof_domain "dodgeball.org"
#set target_email "wilkin@rose-hulman.edu"
#set target_name "Aaron Wilkin"
set target_email "sappon@rose-hulman.edu"
set target_name "Owen Sapp"
#arbitrary
set boundary "asdhqjnnlxzuhcipasd" 
set subject "CSSE432 Homework 2 Owen Sapp"

set domain_expiry "Date Rose-Hulman.edu Domain Expires: 31-Jul-2024\n"
set stack_trace "Commands used to find hackthissite.org\n \
                dig . NS \n \
                dig +norec @a.root-server.net www<dot>hackthissite<dot>org \n \
                dig +norec @d0.org.afilias-nst.org www<dot>hackthissite<dot>org \n \
                dig +norec @h.ns.buddyns.com www<dot>hackthissite<dot>org \n \
                This returns the ip of 137.74.187.103\n\n"
set auth_name "The IP address of Rose-Hulmans Authoritative DNS (dna.rose-hulman.edu) is 137.112.4.196 (done via dig rose-hulman.edu NS)\n\n"
set question "Why would using DNS seem to be \"better?\"  Why would using DNS almost always work faster and HTTP redirects slower?\n\n\
              A DNS is used for optimization in routing for CDNs over HTTP because of increased reliability and efficiency. \
              It almost always is faster than HTTP because DNS utilizes a domain-level protocol while HTTP uses an application level protocol.\
              With a DNS, the request gets a list of servers that may have it, going through the list and checking which has access to the content and is not overloaded. \
              With HTTP, the request is sent through a DNS to a request router which knows where the information is on an application level. This requires significantly more overhead storage data and a longer time per response due to anadditional layer of communication."
set stuff "Yarhar, I love playing dodgeball and searching for buried treasure. X marks the spot and all that, aye? \n Oh well better be off now I have to win back the gym from Davey Jones."
set stuff1 " Yar har, fiddle de dee \n Being a pirate is alright to be \n Do what you want cause a pirate is free \n You are a pirate!\n"

set body "\n $domain_expiry\n\n$stack_trace\n$auth_name\n$question\n\n\n$stuff\n\n$stuff1"

#set fp [open output.txt r]
#set base [read $fp]
#close $fp

set image1 "image.png"
set image2 "image2.gif"
set image1_base [exec cat $image1 | base64]
set image2_base [exec cat $image2 | base64]

set now [clock seconds]
set date [clock format $now -format {%a, %d %b %H:%M:%S}]


spawn telnet rosehulman-edu01b.mail.protection.outlook.com 25
sleep 1
send "HELO $spoof_domain\r"
expect "25? *"
send "MAIL FROM: <$spoof_email>\r"
expect "250 2.1.0 Sender OK"
send "RCPT TO: <$target_email>\r"
expect "250 2.1.5 Recipient OK"
send "DATA\r"
expect "3?? *"

#Header
send "MIME-Version: 1.0 (mime-construct 1.9)\r"
send "Message-ID: <4b739112-3ed9-4c6c-9533-9ce9ff489f61@$spoof_domain>\r"
send "To: \"$target_name\" <$target_email>\r"
send "From: \"$spoof_name\" <$spoof_email>\r"
send "Date: $date -0400\r"
send "Subject: $subject\r"
send "Content-Type: multipart/mixed; boundary=\"$boundary\"\r"
send "\r"
send -- "--$boundary\r"
send "Content-Type: text/plain; charset=UTF-8; format=flowed\r"
send "Content-Disposition: inline\r"
send "\r"
send "$body\r"
send "\r"
send "\r"

#Mime attachment
send -- "--$boundary\r"
send "Content-Type: image/png; name = \"image1.png\"\r"
send "Content-Transfer-Encoding: base64\r"
send "Content-Dispositon: attachment; filename=\"image1.png\"\r"
send "\r"
send -- $image1_base
send "\r"
send "\r"
# anotha one
send -- "--$boundary\r"
send "Content-Type: image/gif; name = \"image2.gif\"\r"
send "Content-Transfer-Encoding: base64\r"
send "Content-Dispositon: attachment; filename=\"image2.gif\"\r"
send "\r"
send -- $image2_base
send "\r"
send "\r"
#send -- "--$boundary\r"
send "\r"

send ".\r"
expect "250 *"
send "QUIT\r"
