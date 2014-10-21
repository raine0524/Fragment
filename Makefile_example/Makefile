#VideoComposite @Author:Ricci.Z 2014.8.14
#
CXX = g++
CXXFLAGS = -Wall -O0 -g -msse4
DEFINES = -DTRANSCODER

TARGET = videocomposite
RELEASE_DIR = ../bin
OBJDIR = ../obj
INCLUDES = -I ../include
LIBS = -L ../lib -lxcoder -lvpx -lnetec -ldts

SRC  = main.cpp
VPATH := $(sort $(dir $(SRC)))
OBJS := $(patsubst %.cpp, $(OBJDIR)/%.o, $(notdir $(SRC)))

$(TARGET): $(OBJS)
	@mkdir -p $(RELEASE_DIR)
	$(CXX) -o $@ $^ $(LIBS);\
	cp $@ $(RELEASE_DIR)


$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

#phony target
.PHONY: clean
clean:
	-rm -rf $(OBJDIR)/* $(TARGET) $(RELEASE_DIR)/$(TARGET)

