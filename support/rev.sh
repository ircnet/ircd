rev=`uname -h 2>&1 | grep usage | grep p | awk ' { print $1 } ' -`
if [ x$rev = x ] ; then
	revdir=`uname -s`-`uname -r|sed -e 's/\([^-]*\).*/\1/'`
else
	revdir=`uname -p`-`uname -s`-`uname -r|sed -e 's/\([^-]*\).*/\1/'`
fi
echo "$revdir" | sed -e 's@/@-@g'
