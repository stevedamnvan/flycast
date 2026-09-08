"""Bounded little-endian consumer tape transport, not source/transform proof.

The future observer must supply actual values. A checksum detects corruption,
not authenticity. This module does not promote supplied metadata to evidence.
"""
import struct
import zlib
from transform_store_inspect import require

MAGIC=b'FC067C01'
HEADER=struct.Struct('<8sII')
RECORD=struct.Struct('<8Q24I')
MAX_RECORDS=3*4096
U64=('sample','epoch','ordinal','cycle','generation','copy_source','copy_destination','decoder_pointer')
U32=('vertex','draw','ta_offset','sq_address','ram_x','ram_y','ram_z','reserved')


def validate(record):
    require(set(record)==set(U64+U32+('before','after')),'consumer record fields')
    for keys,bits in ((U64,64),(U32,32)):
        require(all(type(record[k]) is int and 0<=record[k]<1<<bits for k in keys),'integer width')
    for key in ('before','after'):
        require(len(record[key])==8 and all(type(v) is int and 0<=v<2**32 for v in record[key]),'packet width')
    require(record['reserved']==0,'reserved field')
    require(record['sample']>0 and record['epoch']>0 and record['generation']>0,'missing identity')
    require(record['copy_source']>0 and record['copy_destination']==record['decoder_pointer']>0,'decoder pointer mismatch')
    require(record['before']==record['after'],'copy bytes differ')
    require(record['after'][0]>>29==7,'not vertex packet')
    require(record['ta_offset']%32==0 and record['ta_offset']<8*1024*1024,'TA offset')
    require(record['sq_address']>>26==0x38 and record['sq_address']%32==0,'SQ address')
    require(record['ram_x']%4==0 and record['ram_y']==record['ram_x']+4
            and record['ram_z']==record['ram_x']+8,'XYZ addresses')


def encode(records):
    require(0<len(records)<=MAX_RECORDS,'record budget')
    payload=bytearray()
    for index,record in enumerate(records,1):
        validate(record)
        require(record['sample']==index,'sample sequence')
        payload.extend(RECORD.pack(*(record[k] for k in U64+U32),*record['before'],*record['after']))
    return HEADER.pack(MAGIC,len(records),zlib.crc32(payload))+payload


def decode(data):
    require(HEADER.size<=len(data)<=HEADER.size+MAX_RECORDS*RECORD.size,'tape byte budget')
    magic,count,crc=HEADER.unpack_from(data)
    require(magic==MAGIC and 0<count<=MAX_RECORDS,'tape header')
    require(len(data)==HEADER.size+count*RECORD.size,'tape extent')
    payload=memoryview(data)[HEADER.size:]
    require(zlib.crc32(payload)==crc,'tape checksum')
    result=[]
    for index,values in enumerate(RECORD.iter_unpack(payload),1):
        record=dict(zip(U64+U32,values[:16]))
        record.update(before=list(values[16:24]),after=list(values[24:32]))
        validate(record);require(record['sample']==index,'sample sequence');result.append(record)
    return dict(records=result,transport_only=True,source_proven=False,
                transform_proven=False,presentation_proven=False)
