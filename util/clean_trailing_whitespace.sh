find $1 -type f -name "$2" -exec sh -c 'for i;do echo "$i" && sed 's/[[:space:]]*$//' "$i">/tmp/.$$ && cat /tmp/.$$ > "$i";done' arg0 {} +
