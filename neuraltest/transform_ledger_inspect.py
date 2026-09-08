"""Bounded lossless transform-event envelope; decoding is not arithmetic proof."""
import zlib
from transform_store_inspect import require

COMPRESSED_LIMIT=8*1024*1024
EXPANDED_LIMIT=128*1024*1024


def decode(data,expanded_limit=EXPANDED_LIMIT):
    require(type(expanded_limit) is int and 1<=expanded_limit<=EXPANDED_LIMIT,'expanded budget')
    require(0<len(data)<=COMPRESSED_LIMIT,'compressed budget')
    stream=zlib.decompressobj()
    try:
        raw=stream.decompress(data,expanded_limit+1)
    except zlib.error as error:
        raise ValueError('invalid compressed ledger') from error
    require(len(raw)<=expanded_limit and not stream.unconsumed_tail,'expanded ledger overflow')
    require(stream.eof and not stream.unused_data,'incomplete or trailing ledger')
    try: text=raw.decode('utf-8')
    except UnicodeDecodeError as error: raise ValueError('ledger text encoding') from error
    require(text.endswith('\n') and '\0' not in text,'ledger line boundary')
    return text
