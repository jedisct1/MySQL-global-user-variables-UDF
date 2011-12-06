# $Id$

SOURCES=udf_global_user_variables.c
TARGET=udf_global_user_variables.so
DESTDIR=/usr/local/lib

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) `mysql_config --cflags` \
		-Wall -W -pthread -shared -fPIC -D_GNU_SOURCE=1 \
		$(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

install: all
	install -b -o 0 -g 0 $(TARGET) $(DESTDIR)/$(TARGET)

uninstall:
	rm $(DESTDIR)/$(TARGET)
