import os
import telebot
import asyncio
import logging
import yt_dlp
from hashlib import md5
from dotenv import load_dotenv
import telebot.async_telebot as async_telebot
from telebot import types
from download_video import download_video
from yandex_upload import upload_to_yandex
from storage_for_links import load_data
from storage_for_resolutions_and_sizes import load_resolution_data, save_resolution_data

data = load_data()
resolution_data = load_resolution_data()
bot_data = {}

load_dotenv()
BOT_TOKEN = os.getenv("BOT_TOKEN") # у меня по-другому не работает, можешь следующую строчку раскомментировать
# BOT_TOKEN = os.environ.get('BOT_TOKEN')
# bot = telebot.TeleBot(BOT_TOKEN)
bot = async_telebot.AsyncTeleBot(BOT_TOKEN)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)

def get_hash(text: str) -> str:
    """Generate a short hash for long strings.
    
    Args:
        text (str): Input string to be hashed
        
    Returns:
        str: First 8 characters of MD5 hash hex digest
    """
    return md5(text.encode()).hexdigest()[:8]

@bot.message_handler(commands=['start'])
async def send_welcome(message: types.Message):
    """Handle /start command and send welcome message.
    
    Args:
        message (types.Message): Message object from user
    """
    await bot.reply_to(
        message,
        "Welcome! The bot is ready to present you relevant lecture.",
        reply_markup=set_main_menu()
    )

def set_main_menu() -> types.ReplyKeyboardMarkup:
    """Create persistent menu with main commands.
    
    Returns:
        types.ReplyKeyboardMarkup: Configured keyboard markup with:
            - /lecture
            - /help
            - /start
    """
    main_menu = types.ReplyKeyboardMarkup(resize_keyboard=True)
    main_menu.add(
        types.KeyboardButton("/lecture"),
        types.KeyboardButton("/help"),
        types.KeyboardButton("/start")
    )
    return main_menu

@bot.message_handler(commands=['lecture'])
async def send_lecture_menu(message: types.Message):
    """Display available courses for selection.
    
    Args:
        message (types.Message): Message object from user
        
    Shows:
        Reply keyboard with available course options
    """
    markup = types.InlineKeyboardMarkup()
    for course in data.keys():
        course_hash = get_hash(course)
        markup.add(types.InlineKeyboardButton(
            course, 
            callback_data=f"course_{course_hash}"
        ))
    await bot.send_message(message.chat.id, "Выберите курс:", reply_markup=markup)

@bot.message_handler(commands=['help'])
async def show_help(message: types.Message):
    """Display help information and available commands.
    
    Args:
        message (types.Message): Incoming message object
    
    Sends:
        Markdown-formatted help text with bot commands description
    """
    help_text = """
📚 Доступные команды:
/start - Перезапустить бота
/lecture - Выбрать лекцию
/help - Показать справку
    """
    await bot.reply_to(message, help_text, reply_markup=set_main_menu())

@bot.callback_query_handler(func=lambda call: call.data.startswith('course_'))
async def handle_course_selection(call: types.CallbackQuery):
    """Handle course selection and display available terms.
    
    Args:
        call (types.CallbackQuery): Callback query object with course data
    
    Processes:
        - Extracts course hash from callback data
        - Validates course existence
        - Displays inline keyboard with available terms
    
    Raises:
        ValueError: If course not found in the dataset
    """
    course_hash = call.data.split('_', 1)[1]
    course = next((c for c in data.keys() if get_hash(c) == course_hash), None)
    
    if not course:
        await bot.answer_callback_query(call.id, "Ошибка: курс не найден")
        return

    markup = types.InlineKeyboardMarkup()
    for term in data[course].keys():
        term_hash = get_hash(term)
        markup.add(types.InlineKeyboardButton(
            term, 
            callback_data=f"term_{get_hash(course)}_{term_hash}"
        ))
    
    await bot.edit_message_text(
        "Выберите семестр:",
        chat_id=call.message.chat.id,
        message_id=call.message.message_id,
        reply_markup=markup
    )

@bot.callback_query_handler(func=lambda call: call.data.startswith('term_'))
async def handle_term_selection(call: types.CallbackQuery):
    """Process term selection and display subject options.
    
    Args:
        call (types.CallbackQuery): Callback query with term data
    
    Workflow:
        1. Parse course and term hashes
        2. Validate course-term combination
        3. Generate inline keyboard with subjects
        4. Update message with subject choices
    
    Exceptions:
        Sends error message if data validation fails
    """

    course_hash, term_hash = call.data.split('_', 1)[1].split('_', 1)
    
    course = next((c for c in data.keys() if get_hash(c) == course_hash), None)
    term = next((t for t in data[course].keys() if get_hash(t) == term_hash), None) if course else None
    
    if not course or not term:
        await bot.answer_callback_query(call.id, "Ошибка: данные не найдены")
        return

    markup = types.InlineKeyboardMarkup()
    for subject in data[course][term].keys():
        subject_hash = get_hash(subject)
        markup.add(types.InlineKeyboardButton(
            subject,
            callback_data=f"subject_{course_hash}_{term_hash}_{subject_hash}"
        ))
    
    await bot.edit_message_text(
        "Выберите предмет:",
        chat_id=call.message.chat.id,
        message_id=call.message.message_id,
        reply_markup=markup
    )

@bot.callback_query_handler(func=lambda call: call.data.startswith('subject_'))
async def handle_subject_selection(call: types.CallbackQuery):
    """Handle subject selection and present topic options.
    
    Args:
        call (types.CallbackQuery): Callback with subject data
    
    Operations:
        - Extracts course, term and subject hashes
        - Verifies data consistency
        - Creates inline keyboard with topics
        - Updates interface with topic choices
    
    Error Handling:
        Notifies user via callback answer if data mismatch occurs
    """

    course_hash, term_hash, subject_hash = call.data.split('_', 1)[1].split('_', 2)
    
    course = next((c for c in data.keys() if get_hash(c) == course_hash), None)
    term = next((t for t in data[course].keys() if get_hash(t) == term_hash), None) if course else None
    subject = next((s for s in data[course][term].keys() if get_hash(s) == subject_hash), None) if term else None
    
    if not course or not term or not subject:
        await bot.answer_callback_query(call.id, "Ошибка: данные не найдены")
        return

    markup = types.InlineKeyboardMarkup()
    for topic in data[course][term][subject].keys():
        topic_hash = get_hash(topic)
        markup.add(types.InlineKeyboardButton(
            topic,
            callback_data=f"topic_{course_hash}_{term_hash}_{subject_hash}_{topic_hash}"
        ))
    
    await bot.edit_message_text(
        "Выберите тему:",
        chat_id=call.message.chat.id,
        message_id=call.message.message_id,
        reply_markup=markup
    )

@bot.callback_query_handler(func=lambda call: call.data.startswith('topic_'))
async def handle_topic_selection(call: types.CallbackQuery):
    """Handle topic selection and show available video resolutions.
    
    Args:
        message (types.Message): Message containing selected topic
        
    Raises:
        KeyError: If requested topic not found in data
        
        RuntimeError: If resolution data is missing
    """

    course_hash, term_hash, subject_hash, topic_hash = call.data.split('_', 1)[1].split('_', 3)
    
    course = next((c for c in data.keys() if get_hash(c) == course_hash), None)
    term = next((t for t in data[course].keys() if get_hash(t) == term_hash), None) if course else None
    subject = next((s for s in data[course][term].keys() if get_hash(s) == subject_hash), None) if term else None
    topic = next((t for t in data[course][term][subject].keys() if get_hash(t) == topic_hash), None) if subject else None
    
    if not course or not term or not subject or not topic:
        await bot.answer_callback_query(call.id, "Ошибка: данные не найдены")
        return

    user_id = call.from_user.id
    bot_data[user_id] = {
        'url': data[course][term][subject][topic],
        'course': course,
        'term': term,
        'subject': subject,
        'topic': topic
    }
    
    resolutions = resolution_data[course][term][subject][topic]
    markup = types.InlineKeyboardMarkup()
    for res in resolutions:
        markup.add(types.InlineKeyboardButton(
            f"{res['resolution']}p ({res['filesize_mb']} MB)",
            callback_data=f"dl_{res['resolution']}"
        ))
    
    await bot.edit_message_text(
        f"📹 {topic}",
        chat_id=call.message.chat.id,
        message_id=call.message.message_id,
        reply_markup=markup
    )

@bot.callback_query_handler(func=lambda call: call.data.startswith('dl_'))
async def handle_video_request(call: types.CallbackQuery):
    """Handle video download request and send file to user.
    
     Args:
         call (types.CallbackQuery): Callback query object
        
     Process:
         1. Validate user session
         2. Download video from YouTube
         3. Show download progress
         4. Upload to Telegram
         5. Handle errors and clean temporary files
     """
    user_id = call.from_user.id
    context = bot_data.get(user_id, {})
    video_path = None
    progress_message = None
    
    try:
        if not context:
            await bot.answer_callback_query(call.id, "Сессия истекла!")
            return

        resolution = int(call.data.split('_')[1])
        url = context['url']
        topic = context['topic']

        progress_message = await bot.send_message(call.message.chat.id, "🔄 Подготовка к загрузке...")
        loop = asyncio.get_event_loop()

        def progress_hook(d):
            """Хук для отслеживания прогресса загрузки"""
            if d['status'] == 'downloading':
                downloaded = d.get('downloaded_bytes', 0)
                total = d.get('total_bytes') or d.get('estimated_total_bytes', 0)
                speed = d.get('speed', 0) or 0
                eta = d.get('eta', 0)

                progress_text = "📥 Загрузка видео:\n"
                
                if total > 0:
                    percent = downloaded / total * 100
                    progress_text += f"▰ {percent:.1f}% • "
                    progress_text += f"{downloaded//1024//1024} MB / {total//1024//1024} MB\n"
                else:
                    progress_text += f"{downloaded//1024//1024} MB\n"
                
                progress_text += f"⚡ {speed//1024} KB/s\n"
                progress_text += f"⏳ Осталось: {eta} сек"

                asyncio.run_coroutine_threadsafe(
                    bot.edit_message_text(
                        progress_text,
                        chat_id=progress_message.chat.id,
                        message_id=progress_message.message_id
                    ),
                    loop
                )

        video_path = await asyncio.to_thread(
            download_video,
            url,
            f"bestvideo[height<={resolution}]+bestaudio/best[height<={resolution}]",
            progress_hook
        )

        await bot.edit_message_text(
            "☁ Загружаю в облако...",
            chat_id=progress_message.chat.id,
            message_id=progress_message.message_id
        )

        public_url = await asyncio.to_thread(upload_to_yandex, video_path)
        
        if not public_url:
            raise ValueError("Ошибка загрузки в облако")

        selected_res = next(r for r in 
            resolution_data[context['course']][context['term']][context['subject']][context['topic']] 
            if r["resolution"] == resolution
        )
        selected_res["cloud_url"] = public_url
        await asyncio.to_thread(save_resolution_data, resolution_data)

        await bot.edit_message_text(
            f"✅ Готово! [Скачать лекцию]({public_url})",
            chat_id=progress_message.chat.id,
            message_id=progress_message.message_id,
            parse_mode="Markdown",
            reply_markup=None
        )

    except Exception as e:

        logger.error(f"Ошибка: {str(e)}", exc_info=True)
        error_msg = f"⚠ Ошибка: {str(e)}"
        
        if progress_message:
            await bot.edit_message_text(
                error_msg,
                chat_id=progress_message.chat.id,
                message_id=progress_message.message_id
            )
        else:
            await bot.send_message(call.message.chat.id, error_msg)

    finally:
        if video_path and os.path.exists(video_path):
            os.remove(video_path)

async def main():
    """Main async entry point for bot operation.
    
    Initializes and maintains bot polling with:
        - Continuous operation (none_stop=True)
        - Async event loop management
    
    Note:
        Should be executed via asyncio.run() when starting the bot
    """
    await bot.polling(none_stop=True)

if __name__ == '__main__':
    logger.info("Bot is running!")
    asyncio.run(main())
       

