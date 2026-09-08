"""Bounded binary32 expression nodes; FTRV leaves require external verification."""
from binary32_oracle import fraction, rounded_bits
from transform_store_inspect import require


def evaluate(node, depth=0):
    require(depth<=64, 'expression depth')
    require(node is not None, 'unknown expression')
    if node[0] in ('literal','verified-ftrv'):return node[-1]
    require(node[0] in ('fadd','fmul','fdiv'), 'expression operation')
    a,b=(fraction(evaluate(n,depth+1)) for n in node[1:])
    if node[0]=='fadd':value=a+b
    elif node[0]=='fmul':value=a*b
    else:
        require(b!=0,'expression divide by zero');value=a/b
    return rounded_bits(value,3)
