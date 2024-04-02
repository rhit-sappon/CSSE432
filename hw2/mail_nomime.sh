#!/usr/bin/expect
#set spoof_name "Steve the Pirate"
#set spoof_email "piratesteve@dodgeball.com"
#set spoof_domain "dodgeball.com"
set spoof_name "Pirate Steve"
set spoof_email "stevep@dodgeball.org"
set spoof_domain "rose-hulman.edu"
set target_email "sappon@rose-hulman.edu"
set target_name "Owen Sapp"

set subject "CSSE 432 Homework 2 Owen Sapp"
set domain_expiry "Date Rose-Hulman.edu Domain Expires: 31-Jul-2024"
set stack_trace ""
set auth_name "The IP address of Rose-Hulmans Authoritative DNS () is "
set question "Why would using DNS seem to be \"better?\"  Why would using DNS almost always work faster and HTTP redirects slower?\n\n\
              A DNS is used for optimization in routing for CDNs over HTTP because of increased reliability and efficiency.\
              It almost always is faster than HTTP because DNS utilizes a domain-level protocol while HTTP uses an application level protocol.\
              With a DNS, the request gets a list of servers that may have it, going through the list and checking which has access to the content and is not overloaded.\
              With HTTP, the request is sent through a DNS to a request router which knows where the information is on an application level. This requires significantly more overhead.\n"
set stuff "Yarhar, I love playing dodgeball and searching for buried treasure."
set body "$domain_expiry\n$stack_trace\n$auth_name\n$question\n\n\n$stuff"

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
send "Message-ID: <4b739112-3ed9-4c6c-9533-9ce9ff489f61@$spoof_domain>\r"
send "To: \"$target_name\" <$target_email>\r"
send "From: \"$spoof_name\" <$spoof_email>\r"
send "Date: $date -0400\r"
send "Subject: $subject\r"
send "Content-Type: text/plain; charset=UTF-8; format=flowed\r"

send "\r"
send "$body\r"
send "\r"
send ".\r"
expect "250 *"
send "QUIT\r"
