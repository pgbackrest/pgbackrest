find . -type f -name "$1" -exec sh -c 'for i;do sed 's/[[:space:]]*$//' "$i">/tmp/.$$ && cat /tmp/.$$ > "$i";done' arg0 {} +
