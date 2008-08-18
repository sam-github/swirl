#!/bin/sh

D=html

T1=/tmp/makedoc$$a
T2=/tmp/makedoc$$b
T3=/tmp/makedoc$$c

if [ ! -d $D ]; then
    echo "$D doesn't exist or isn't a directory..." 1>&2
    exit 1
fi
(cd $D ; rm -f *)

X=/usr/pkg/share/www/xml/etc/xml2rfc.tcl
for A in Architecture.xml ProfileTutorial; do
    echo "source $X; xml2html $A; exit" | tclsh
done
cp Architecture.html ProfileTutorial.html $D

/usr/local/bin/doc++ --dir $D --html --quick doc++.dxx

cd $D

for A in *.html; do
    case "$A" in
        Architecture.html|ProfileTutorial.html)
	    ;;

        *)  sed -e 's%externstruct%extern struct%g' < $A | \
              sed -e s'%<A HREF="profile.html">profile</A>-%profile-%g' | \
              sed -e s'%<A HREF="index.html">%<A HREF="index.html">Home</A> <A HREF="index2.html">%g' | \
                tee $T1                                  | \
	        sed -e 's%<A HREF="#DOC.DOCU">%%g'       | \
	        fgrep '<A HREF='                         | \
	        sed -e "s%<A HREF=\"#%<A HREF=\"$A#%g"   | \
		awk '{
    if (match($0, "<A HREF=[^\"].*>[^<]*</A>") > 0) {
	entry = substr($0, RSTART, RLENGTH);

        href = substr(entry, 9);
	href = substr(href, 1, index(href,">") - 1);
    } else if (match($0, "<A HREF=\".*\">[^<]*</A>") > 0) {
	entry = substr($0, RSTART, RLENGTH);

        href = substr(entry, 10);
	href = substr(href, 1, index(href,"\"") - 1);
    } else next;
    text = substr(entry, index(entry, ">") + 1);
    text = substr(text, 1, index(text, "</A>")-1);

    if (index(text, " ")                 \
            || index(text, "-")          \
            || index(text, "threaded_os")) next;
    if ((text == "BEEP")                 \
            || (text == "DOC++")         \
            || (text == "callback")      \
            || (text == "Configuration") \
            || (text == "connection")    \
            || (text == "Functions")     \
            || (text == "Home")          \
            || (text == "Logging")       \
            || (text == "Semaphores")    \
            || (text == "Threads")       \
            || (text == "TLS")) next;

    if (match(text, "<B>.*</B>")) text = substr(text, 4, length(text) - 7);

    key = text;
    if (substr(key, 1, 2) == "(*") key = substr(key, 3);
    for (i = 1; i <= length(key); i++) {
        c = substr(key, i, 1);
	if (c == "_") c = " ";
	printf "%s", c;
    }

    printf "\t%s\t%s\n", text, href;
}' >> $T2

	    cp $T1 $A
	    ;;
    esac
done

sort -f < $T2 | \
    uniq       | \
    tee $T3    | \
    awk -F'	' '
BEGIN   {
    printf "<html><head><title>Alphabetic Index</title></head>\n"
    printf "<body bgcolor=\"#ffffff\">\n"
    printf "<h2>Alphabetic Index</h2><blockquote><b>"
}
        {
            first = tolower(substr($2,1,1));
	    if (first == "(") first = tolower(substr($2,3,1));
	    if (exists[first] == "") {
		exists[first] = 1;

                printf "<a href=\"#index.%s\">%s</a>\n", first, toupper(first);
            }
        }
END     {
    printf "</b></blockquote><hr>\n"
}' > $T1

    awk -F'	' '
BEGIN   {
    printf "<table>\n"
}
        {
            first = tolower(substr($2,1,1));
	    if (first == "(") first = tolower(substr($2,3,1));
	    if (exists[first] == "") {
		exists[first] = 1;

                printf "<tr><td><b><a name=\"#index.%s\">%s</a></b></td>\n",
	               first, toupper(first);
            } else {
                printf "<tr><td></td>"
            }
            printf "<td><a href=\"%s\">%s</a></td></tr>\n", $3, $2;
        }
END     {
    printf "</table>\n"
    printf "<i><a href=\"index.html\">Home</a> <a href=\"index2.html\">Alphabetic index</a></i><hr>\n"
    printf "</body></html>\n"
}' < $T3 >> $T1

cp $T1 index2.html

rm -f $T1 $T2 $T3

tar cf - *.gif *.html | gzip > docs.tgz
zip docs.zip *.gif *.html > /dev/null

echo "cd $D ; scp * \$USER@\$HTDOCS/"

exit
