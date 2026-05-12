registerFileType((fileExt, filePath, fileData) => {
    // Check for locs extension
    if (fileExt == 'locs') {
        const headerArray = fileData.getBytesAt(0, 4);
        const header = String.fromCharCode(...headerArray)
        if (header == 'RIFF')
            return true;
    }
    return false;
});