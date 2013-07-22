#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>

#import "EAGLView.h"

#define USE_DEPTH_BUFFER 1

// A class extension to declare private methods
@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;

@end

@implementation EAGLView

@synthesize context;

// You must implement this
+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

//The GL view is stored in the nib file. When it's unarchived it's sent -initWithCoder:
- (id)initWithCoder:(NSCoder*)coder
{
    if ((self = [super initWithCoder:coder])) {
        // Get the layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties =
        [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:NO],
         kEAGLDrawablePropertyRetainedBacking,
         kEAGLColorFormatRGBA8,
         kEAGLDrawablePropertyColorFormat, nil];
        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
        if (!context || ![EAGLContext setCurrentContext:context]) {
            //[self release];
            return nil;
        }
        [self copyRes];
        //NSString *ressourcePath = [[NSBundle mainBundle] resourcePath];
        NSString *startupModel = [docPath stringByAppendingPathComponent:@"pmsm.geo"];

        mContext = new drawContext();
        mContext->load(*new std::string([startupModel fileSystemRepresentation]));
    }
    return self;
}

- (void) copyRes
{
    NSString *resPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"files"];
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    docPath = [paths objectAtIndex:0]; //Get the docs directory
    
    NSArray *resContents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:resPath error:NULL];
    
    for (NSString* obj in resContents){
        NSError* error;
        if (![[NSFileManager defaultManager] copyItemAtPath:[resPath stringByAppendingPathComponent:obj] toPath:[docPath stringByAppendingPathComponent:obj] error:&error])
            NSLog(@"Error: %@", error);;
    }
}

- (void)drawView
{
    [EAGLContext setCurrentContext:context];
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glViewport(0, 0, backingWidth, backingHeight); // need this ...??
    mContext->initView(backingWidth, backingHeight);
    mContext->drawView();
    
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
}
- (void)loadMsh:(NSString*) file
{
    NSString *msh = [docPath stringByAppendingPathComponent: file];
    //mContext = new drawContext();
    mContext->load(*new std::string([msh fileSystemRepresentation]));
    [self drawView];
}
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    NSUInteger ntouch = [[event allTouches] count];
    UITouch* touch = [touches anyObject];
    CGPoint position = [touch locationInView:self];
    CGPoint lastPosition = [touch previousLocationInView:self];
    switch(ntouch)
    {
        case 1:
        {
            mContext->eventHandler(1,position.x,position.y);
        }
            break;
        case 3:
        {
            mContext->eventHandler(3,position.x,position.y);
        }
            break;
        default:
            return ;
    }
    
    [self drawView];
}

- (void)layoutSubviews
{
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [self createFramebuffer];
    [self drawView];
}

- (BOOL)createFramebuffer
{
    glGenFramebuffersOES(1, &viewFramebuffer);
    glGenRenderbuffersOES(1, &viewRenderbuffer);
	
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
    
    if (USE_DEPTH_BUFFER) {
        glGenRenderbuffersOES(1, &depthRenderbuffer);
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
        glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, backingWidth, backingHeight);
        glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);
    }
    
    if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
        return NO;
    }
    
    return YES;
}

- (void)destroyFramebuffer
{
    glDeleteFramebuffersOES(1, &viewFramebuffer);
    viewFramebuffer = 0;
    glDeleteRenderbuffersOES(1, &viewRenderbuffer);
    viewRenderbuffer = 0;
    if(depthRenderbuffer) {
        glDeleteRenderbuffersOES(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
}

- (void)dealloc
{
    if ([EAGLContext currentContext] == context) {
        [EAGLContext setCurrentContext:nil];
    }
}

@end
