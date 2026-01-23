import yt_dlp
import logging
import os

def download_video(url, format_selector, progress_callback=None):
    """Download a video from YouTube using specified format parameters.
    
    Args:
        url (str): URL of the YouTube video to download
        format_str (str): yt-dlp format selector string (e.g., 'bestvideo+bestaudio/best')
        
    Returns:
        str: Full path to the downloaded video file
        
    Raises:
        yt_dlp.DownloadError: If video download fails
        OSError: If there are file system errors
        
    """
    # output_template = "downloads/%(title)s.%(ext)s"
    output_template = "~/mipt_lecture_project/Downloads/%(id)s.%(ext)s"
    os.makedirs("downloads", exist_ok=True)

    ydl_opts = {
        'format': format_selector,
        'progress_hooks': [progress_callback] if progress_callback else [],
        'outtmpl': output_template,
        'quiet': True,
        'noplaylist': True,
        # 'cookiesfrombrowser': ('chrome',)
        'cookiesfrombrowser': ('firefox',)
    }
    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        info = ydl.extract_info(url, download=True)
        return ydl.prepare_filename(info)
