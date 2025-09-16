float distort(float sample, float dist_level, float flip_level)
{

    if (sample > flip_level)
        return sample - 2 * (sample - flip_level);
    if (sample < -flip_level)
        return sample - 2 * (sample + flip_level);

    if (sample > dist_level)
        return dist_level;
    if (sample < -dist_level)
        return -dist_level;

    return sample;
}
