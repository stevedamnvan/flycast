"""Recorded Z-divided projection, not a world-camera recovery."""
from binary32_oracle import fraction, rounded_bits
from transform_store_inspect import require


def project(point, matrix, numerator, constants, denominator=2):
    require(len(point)==4 and len(matrix)==16, 'projection input shape')
    dot=[rounded_bits(sum(fraction(matrix[4*j+i])*fraction(point[j]) for j in range(4)),3) for i in range(4)]
    require(denominator in (2,3) and fraction(dot[denominator])!=0, 'projection denominator')
    reciprocal=rounded_bits(fraction(numerator)/fraction(dot[denominator]),3)
    mul=lambda a,b:rounded_bits(fraction(a)*fraction(b),3)
    add=lambda a,b:rounded_bits(fraction(a)+fraction(b),3)
    return [add(mul(dot[0],reciprocal),constants[24]),
            add(mul(dot[1],reciprocal),constants[25]),mul(reciprocal,constants[20])]


def inspect(details):
    count=wrong_denominator=wrong_offset=0
    for row in details:
        if row['transform_id'][1]!='8c03a9ea': continue
        args=(row['point_words'],row['matrix_words'],row['numerator_word'],row['output_constants'])
        require(args[2] is not None, 'missing observed numerator')
        require(project(*args)==row['xyz_words'], 'projection XYZ mismatch')
        try: bad=project(*args,denominator=3)
        except ValueError: wrong_denominator+=1
        else:
            require(bad!=row['xyz_words'],'wrong W denominator accepted')
            wrong_denominator+=1
        changed=dict(args[3]);changed[24]=rounded_bits(fraction(changed[24])+1,3)
        require(project(*args[:3],changed)!=row['xyz_words'],'wrong offset accepted')
        wrong_offset+=1;count+=1
    require(count>0,'no divided records')
    return dict(exact_projected_records=count,wrong_denominator_rejections=wrong_denominator,
                wrong_offset_rejections=wrong_offset,world_camera_recovered=False)
