# File we have to transform locally.
LOCAL_FILES=\
	proto/routerctl.proto

# File we have to load from the DFD
DFD_PROTO_DIR=../../dfd_development/include/proto
INCLUDED_FILES=\
	$(DFD_PROTO_DIR)/status.proto

PROTO_FILES=$(foreach v,$(LOCAL_FILES) $(INCLUDED_FILES),$(notdir $(subst .proto,_pb2.py,$v))) \
            $(foreach v,$(LOCAL_FILES) $(INCLUDED_FILES),$(notdir $(subst .proto,_pb2_grpc.py,$v)))

all: $(PROTO_FILES)

.PHONY: clean

# Can't figure out how to make just one implicit rule.

%_pb2.py %_pb2_grpc.py: proto/%.proto
	python3 -m grpc_tools.protoc \
			-I$(DFD_PROTO_DIR)\
			-Iproto \
			--python_out=. \
			--grpc_python_out=. \
			$<

%_pb2.py %_pb2_grpc.py: $(DFD_PROTO_DIR)/%.proto
	python3 -m grpc_tools.protoc \
			-I$(DFD_PROTO_DIR)\
			-Iproto \
			--python_out=. \
			--grpc_python_out=. \
			$<

clean:
	-rm $(PROTO_FILES)
